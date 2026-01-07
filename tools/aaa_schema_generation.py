import os
import re
import json
import argparse
from copy import deepcopy
from collections import deque, defaultdict
from pathlib import Path
from typing import Dict, Any, Optional, Set, List, Tuple

def extract_classname_from_constructor(code: str) -> Optional[str]:
    match = re.search(r'if\s*\(\s*str\s*==\s*"([^"]+)"\s*\)', code)
    return match.group(1) if match else None

def load_documentation_json(doc_path: Path) -> Dict[str, Any]:
    if not doc_path.exists():
        print(f"[WARN] Documentation file not found: {doc_path}")
        return {}
    try:
        with open(doc_path, 'r', encoding='utf-8') as f: return json.load(f)
    except Exception as e:
        print(f"[ERROR] Failed to load documentation JSON: {e}")
        return {}

def _get_priority_field_schema() -> Dict[str, Any]:
    return { "type": "array", "items": { "type": "array", "items": [ {"type": "array", "items": {"type": "number"}, "minItems": 2, "maxItems": 2, "description": "Shutter range [Tmin, Tmax] in microseconds"}, {"type": "array", "items": {"type": "integer"}, "minItems": 2, "maxItems": 2, "description": "Gain range [Gmin, Gmax] as scalar multiplier"}, {"type": "array", "items": {"type": "number"}, "minItems": 2, "maxItems": 2, "description": "Iris range [Imin, Imax] as fnum values"}, {"type": "array", "items": {"type": "string"}, "description": "Priority order array like ['shutter', 'gain', 'iris']"} ], "minItems": 4, "maxItems": 4 } }

def parse_doc_type_to_schema(doc_type: str, doc_range: str = "") -> Dict[str, Any]:
    doc_type = doc_type.lower().strip()
    if doc_type in ['boolean', 'bool']: return {"type": "boolean"}
    if doc_type in ['int', 'integer']:
        schema = {"type": "integer"}
        if doc_range and '[' in doc_range and ']' in doc_range:
            try:
                match = re.search(r'\[([^,]+),([^\]]+)\]', doc_range)
                if match:
                    schema["minimum"] = int(match.group(1).strip())
                    schema["maximum"] = int(match.group(2).strip())
            except (ValueError, IndexError): pass
        return schema
    if doc_type in ['float', 'double', 'number']:
        schema = {"type": "number"}
        try:
            if '[' not in doc_range and ',' in doc_range: schema["enum"] = [float(eval(v.strip())) for v in doc_range.split(',')]
            elif '[' in doc_range and ']' in doc_range:
                match = re.search(r'\[([^,]+),([^\]]+)\]', doc_range)
                if match:
                    schema["minimum"] = float(eval(match.group(1).strip()))
                    schema["maximum"] = float(eval(match.group(2).strip()))
        except (ValueError, SyntaxError, NameError, TypeError): pass
        return schema
    if doc_type in ['string', 'str']: return {"type": "string"}
    if doc_type.startswith('vector<') and doc_type.endswith('>'):
        return {"type": "array", "items": parse_doc_type_to_schema(doc_type[7:-1].strip(), doc_range)}
    if "vector<vector<" in doc_type:
        match = re.search(r'vector<vector<(\w+)>>', doc_type)
        if match:
            base_schema = parse_doc_type_to_schema(match.group(1), doc_range)
            return {"anyOf": [{"type": "array", "items": base_schema}, {"type": "array", "items": {"type": "array", "items": base_schema}}]}
    return {}

def get_doc_schema_for_field(field_path: str, classname: str, doc_data: Dict[str, Any]) -> Optional[Dict[str, Any]]:
    if not doc_data or classname not in doc_data or "table_values" not in doc_data.get(classname, {}): return None
    for row in doc_data[classname]["table_values"]:
        if len(row) >= 3 and row[0] == field_path:
            _, description, field_type, *rest = row
            field_range = rest[0] if rest else ""
            leaf_name = field_path.split('.')[-1]
            schema = _get_priority_field_schema() if leaf_name in ['priority', 'priorityHdr'] else parse_doc_type_to_schema(field_type, field_range)
            if schema and description: schema["description"] = description
            return schema
    return None

def get_documented_fields_structured(classname: str, doc_data: Dict[str, Any]) -> Dict[str, Dict[str, Any]]:
    if not doc_data or classname not in doc_data or "table_values" not in doc_data.get(classname, {}): return {}
    fields = {}
    for row in doc_data[classname]["table_values"]:
        if len(row) >= 3:
            path, desc, ftype, *rest = row[0], row[1], row[2], row[3:]
            frange = rest[0] if rest else ""
            leaf = path.split('.')[-1]
            schema = _get_priority_field_schema() if leaf in ['priority', 'priorityHdr'] else parse_doc_type_to_schema(ftype, frange)
            if schema and desc: schema["description"] = desc
            fields[path] = schema
    return fields

def identify_nested_objects_from_code(code: str) -> Set[str]:
    confirmed = set()
    patterns = re.findall(r'(?:auto\s*&|Json::Value)\s+(\w+)\s*=\s*node\["([^"]+)"\]', code)
    for var, field in patterns:
        if re.search(rf'{var}\["', code):
            confirmed.add(field)
    return confirmed

def get_leaf_fields_from_schema(schema: Dict[str, Any], prefix: str = "") -> Set[str]:
    leaves = set()
    if not isinstance(schema, dict): return leaves
    if "oneOf" in schema and isinstance(schema["oneOf"], list):
        for sub_schema in schema["oneOf"]:
            leaves.update(get_leaf_fields_from_schema(sub_schema, prefix))
    if "properties" not in schema: return leaves
    for name, prop in schema["properties"].items():
        path = f"{prefix}.{name}" if prefix else name
        if isinstance(prop, dict):
            if prop.get("type") == "object" and "properties" in prop:
                leaves.update(get_leaf_fields_from_schema(prop, path))
            elif prop.get("type") == "array" and isinstance(prop.get("items"), dict) and prop["items"].get("type") == "object":
                leaves.update(get_leaf_fields_from_schema(prop["items"], f"{path}.items"))
            else:
                leaves.add(path)
        else:
            leaves.add(path)
    return leaves

def is_field_implemented_in_schema(field_path: str, schema: Dict[str, Any]) -> bool:
    def search_in_schema(current_schema: Dict[str, Any], path_parts: List[str]) -> bool:
        if not path_parts or not isinstance(current_schema, dict): return False
        key = path_parts[0]
        if key == 'items':
             if current_schema.get("type") == "array" and "items" in current_schema:
                return search_in_schema(current_schema["items"], path_parts[1:])
             return False
        if "properties" in current_schema and key in current_schema["properties"]:
            if len(path_parts) == 1: return True
            return search_in_schema(current_schema["properties"][key], path_parts[1:])
        return False
    if search_in_schema(schema, [field_path]): return True
    parts = field_path.split('.')
    if len(parts) > 1 and search_in_schema(schema, parts): return True
    leaf = parts[-1]
    for path in get_leaf_fields_from_schema(schema):
        if path.endswith(f".{leaf}"): return True
    return False
    
def deep_merge(source: Dict, destination: Dict) -> Dict:
    for key, value in source.items():
        if isinstance(value, dict) and key in destination and isinstance(destination.get(key), dict):
            destination[key] = deep_merge(value, destination[key])
        else:
            destination[key] = value
    return destination

def add_documented_nested_structures(schema: Dict[str, Any], classname: str, doc_data: Dict[str, Any], confirmed_nested_objects: Set[str]) -> int:
    if not doc_data or classname not in doc_data: return 0
    doc_fields = get_documented_fields_structured(classname, doc_data)
    additions = 0
    for path, field_schema in doc_fields.items():
        if '.' in path:
            parts = path.split('.')
            parent, child = parts[0], parts[-1]
            if parent in confirmed_nested_objects and not is_field_implemented_in_schema(path, schema):
                if parent not in schema["properties"]: schema["properties"][parent] = {}
                if "type" not in schema["properties"][parent]: schema["properties"][parent]["type"] = "object"
                current = schema["properties"][parent]
                if current.get("type") != "object": continue
                if "properties" not in current: current["properties"] = {}
                current["properties"][child] = field_schema.copy()
                additions += 1
    return additions

def refine_existing_fields_with_documentation(schema: Dict[str, Any], classname: str, doc_data: Dict[str, Any]) -> int:
    if not doc_data or classname not in doc_data: return 0
    refinements = 0
    def refine_recursive(current_schema: Dict[str, Any], path_prefix: str = ""):
        nonlocal refinements
        if not isinstance(current_schema, dict) or "properties" not in current_schema: return
        for name, prop in list(current_schema["properties"].items()):
            path = f"{path_prefix}.{name}" if path_prefix else name
            doc_schema = get_doc_schema_for_field(path, classname, doc_data) or get_doc_schema_for_field(name, classname, doc_data)
            if doc_schema:
                before = json.dumps(prop, sort_keys=True)
                deep_merge(doc_schema, prop)
                if before != json.dumps(prop, sort_keys=True): refinements += 1
            if isinstance(prop, dict):
                if prop.get("type") == "object": refine_recursive(prop, path)
                elif prop.get("type") == "array" and isinstance(prop.get("items"), dict) and prop["items"].get("type") == "object":
                     refine_recursive(prop["items"], f"{path}.items")
    refine_recursive(schema)
    return refinements

def set_required_fields(schema: Dict[str, Any], fields_to_require: Set[str]):
    if not isinstance(schema, dict) or not fields_to_require: return
    required = {f for f in fields_to_require if f in schema.get("properties", {})}
    if required:
        existing_required = set(schema.get("required", []))
        schema["required"] = sorted(list(existing_required.union(required)))

def generate_field_comparison_report(classname: str, schema: Dict[str, Any], doc_data: Dict[str, Any]) -> Tuple[Set[str], Set[str], Set[str]]:
    schema_fields = get_leaf_fields_from_schema(schema)
    doc_fields = set(get_documented_fields_structured(classname, doc_data).keys())
    schema_leaf_map = {f.split('.')[-1]: f for f in schema_fields}
    implemented_schema_paths = set()
    implemented_doc_paths = set()
    code_not_doc = set(schema_fields)
    for doc_path in doc_fields:
        if doc_path in schema_fields:
            implemented_schema_paths.add(doc_path)
            implemented_doc_paths.add(doc_path)
            if doc_path in code_not_doc:
                code_not_doc.remove(doc_path)
            continue
        leaf = doc_path.split('.')[-1]
        if leaf in schema_leaf_map:
            full_schema_path = schema_leaf_map[leaf]
            implemented_schema_paths.add(full_schema_path)
            implemented_doc_paths.add(doc_path)
            if full_schema_path in code_not_doc:
                code_not_doc.remove(full_schema_path)
    doc_not_code = doc_fields - implemented_doc_paths
    code_not_doc = {f for f in code_not_doc if f.split('.')[-1] not in {"classname", "forcecreate", "enable", "disable"}}
    return code_not_doc, doc_not_code, implemented_schema_paths

def is_field_obsolete(field: str, classname: str, doc: Dict[str, Any]) -> bool:
    if not doc or classname not in doc or "table_values" not in doc.get(classname, {}): return False
    for row in doc[classname]["table_values"]:
        if len(row) >= 2 and row[0] == field:
            if "obsolete" in row[1].lower():
                return True
    return False

def refine_schema_recursively(schema: Dict[str, Any], classname: str, doc_data: Dict[str, Any], all_implemented_fields: Set[str], optional_string_fields: Set[str], prefix: str = ""):
    for prop_name, prop_schema in schema.get("properties", {}).items():
        if not isinstance(prop_schema, dict): continue
        prop_path = f"{prefix}.{prop_name}" if prefix else prop_name
        if prop_schema.get("type") == "object":
            refine_schema_recursively(prop_schema, classname, doc_data, all_implemented_fields, optional_string_fields, prop_path)
        elif prop_schema.get("type") == "array" and prop_schema.get("items", {}).get("type") == "object":
            items_prefix = f"{prop_path}.items"
            refine_schema_recursively(prop_schema["items"], classname, doc_data, all_implemented_fields, optional_string_fields, items_prefix)

    pairs_info = []
    if doc_data and classname in doc_data and "table_values" in doc_data.get(classname, {}):
        pattern = re.compile(r'\(obsolete\s*-\s*prefer\s*using\s+([\w_]+)\)', re.IGNORECASE)
        for row in doc_data[classname]["table_values"]:
            if len(row) >= 2:
                doc_path, description = row[0], row[1]
                match = pattern.search(description)
                if match:
                    new_field_leaf = match.group(1)
                    obsolete_field_leaf = doc_path.split('.')[-1]
                    pairs_info.append((obsolete_field_leaf, new_field_leaf))
    
    obsolete_pairs_at_this_level = []
    current_properties = set(schema.get("properties", {}).keys())
    for obsolete_leaf, new_leaf in pairs_info:
        if obsolete_leaf in current_properties and new_leaf in current_properties:
            obsolete_pairs_at_this_level.append((obsolete_leaf, new_leaf))
    
    handled_in_oneof = set()
    if obsolete_pairs_at_this_level:
        if 'oneOf' not in schema: schema['oneOf'] = []
        for obsolete_field, new_field in obsolete_pairs_at_this_level:
            if obsolete_field in schema["properties"] and new_field in schema["properties"]:
                obsolete_prop = schema["properties"].pop(obsolete_field)
                new_prop = schema["properties"].pop(new_field)
                schema['oneOf'].extend([ {"properties": {obsolete_field: obsolete_prop}, "required": [obsolete_field]}, {"properties": {new_field: new_prop}, "required": [new_field]} ])
                handled_in_oneof.update([obsolete_field, new_field])

    level_fields = {f for f in all_implemented_fields if ".".join(f.split('.')[:-1]) == prefix}
    fields_to_make_required = set()
    for f_path in level_fields:
        leaf_name = f_path.split('.')[-1]
        if (leaf_name not in handled_in_oneof and
                not is_field_obsolete(f_path, classname, doc_data) and
                leaf_name not in optional_string_fields):
            fields_to_make_required.add(leaf_name)
            
    set_required_fields(schema, fields_to_make_required)

def add_optional_documented_strings(schema: Dict[str, Any], classname: str, doc_data: Dict[str, Any]) -> Set[str]:
    """Adds documented-only string fields to the schema as optional properties and returns their names."""
    _, doc_not_code, _ = generate_field_comparison_report(classname, schema, doc_data)
    added_fields = set()
    for field_path in doc_not_code:
        if '.' in field_path:
            continue
        
        doc_schema = get_doc_schema_for_field(field_path, classname, doc_data)
        
        if doc_schema and doc_schema.get("type") == "string":
            if field_path not in schema["properties"]:
                schema["properties"][field_path] = doc_schema
                added_fields.add(field_path)
    return added_fields

def analyze_json_access_patterns(code: str, classname: str = "", doc_data: Dict[str, Any] = None) -> Tuple[Dict[str, Any], Dict[str, Any]]:
    type_map = {'Bool': 'boolean', 'Float': 'number', 'Double': 'number', 'Int': 'integer', 'UInt': 'integer', 'String': 'string', 'CString': 'string'}
    cleaned_code = remove_disabled_code_blocks(code)
    schema = {"type": "object", "properties": {}}
    
    confirmed_nested_objects = identify_nested_objects_from_code(cleaned_code)
    direct_patterns = re.findall(r'node\["([^"]+)"\]\.as(\w+)\(\)', cleaned_code)
    for field, ctype in direct_patterns:
        schema["properties"][field] = {"type": type_map.get(ctype, "string")}
    conditional_patterns = re.findall(r'if\s*\(\s*node\.isMember\(\s*"([^"]+)"\s*\)\s*\)', cleaned_code)
    for field in conditional_patterns:
        if field not in schema["properties"]: schema["properties"][field] = {} 
    func_param_patterns = re.findall(r'[(,]\s*node\["([^"]+)"\]', cleaned_code)
    for field in func_param_patterns:
        if field not in schema["properties"]: schema["properties"][field] = {} 
    array_patterns = re.findall(r'Json::Value\s+(\w+)\s*=\s*node\["([^"]+)"\]', cleaned_code)
    for var_name, field_name in array_patterns:
        if re.search(rf'{var_name}\.isArray\(\)|for\s*\(.*:\s*{var_name}\)', code):
            sub_field_access = re.findall(rf'{var_name}\[\s*\w+\s*\]\["([^"]+)"\]', code)
            if sub_field_access:
                schema["properties"][field_name] = {"type": "array", "items": {"type": "object", "properties": {}}}
                for sub_field in set(sub_field_access):
                    sub_schema, is_2d = {}, False
                    assigned_var_match = re.search(rf'Json::Value\s+(\w+)\s*=\s*{re.escape(var_name)}\[\s*\w+\s*\]\["{re.escape(sub_field)}"\]', code)
                    match_2d = (re.search(rf'{re.escape(var_name)}\[\s*\w+\s*\]\["{re.escape(sub_field)}"\]\[\s*\w+\s*\]\[\s*\w+\s*\]\.as(\w+)\(\)', code) or
                                (assigned_var_match and re.search(rf'{re.escape(assigned_var_match.group(1))}\[\s*\w+\s*\]\[\s*\w+\s*\]\.as(\w+)\(\)', code)))
                    if match_2d:
                        is_2d = True
                        sub_schema = {"type": "array", "items": {"type": "array", "items": {"type": type_map.get(match_2d.group(1), "string")}}}
                    if not is_2d:
                        match_1d = (re.search(rf'{re.escape(var_name)}\[\s*\w+\s*\]\["{re.escape(sub_field)}"\]\[\s*\w+\s*\]\.as(\w+)\(\)', code) or
                                    (assigned_var_match and re.search(rf'{re.escape(assigned_var_match.group(1))}\[\s*\w+\s*\]\.as(\w+)\(\)', code)))
                        if match_1d:
                            sub_schema = {"type": "array", "items": {"type": type_map.get(match_1d.group(1), "string")}}
                    schema["properties"][field_name]["items"]["properties"][sub_field] = sub_schema
            else:
                primitive_match = re.search(rf'{re.escape(var_name)}\[\s*\w+\s*\]\.as(\w+)\(\)', code)
                if primitive_match:
                    schema["properties"][field_name] = {"type": "array", "items": {"type": type_map.get(primitive_match.group(1), "string")}}
                elif field_name not in schema["properties"]:
                    schema["properties"][field_name] = {"type": "array"}

    additions = add_documented_nested_structures(schema, classname, doc_data, confirmed_nested_objects)
    refinements = refine_existing_fields_with_documentation(schema, classname, doc_data)
    
    optional_string_fields = add_optional_documented_strings(schema, classname, doc_data)
    additions += len(optional_string_fields)
    
    _, _, all_implemented_fields = generate_field_comparison_report(classname, schema, doc_data)
    refine_schema_recursively(schema, classname, doc_data, all_implemented_fields, optional_string_fields)
    
    code_not_doc, doc_not_code, implemented_fields_final = generate_field_comparison_report(classname, schema, doc_data)
    report = {"classname": classname, "total_schema_fields": len(get_leaf_fields_from_schema(schema)), "total_documented_fields": len(get_documented_fields_structured(classname, doc_data)), "matching_fields": len(implemented_fields_final), "fields_in_code_not_in_doc": sorted(list(code_not_doc)), "fields_in_doc_not_in_code": sorted(list(doc_not_code)), "refinements_applied": refinements, "additions_made": additions, "matching_fields_list": sorted(list(implemented_fields_final))}
    return schema, report

def remove_disabled_code_blocks(code: str) -> str:
    return re.sub(r'#if\s+0.*?#endif', '', code, flags=re.DOTALL)

def print_detailed_comparison_report(report: Dict[str, Any]):
    print(f"\n{'='*60}\nFIELD COMPARISON REPORT: {report['classname']}\n{'='*60}")
    print(f"Schema fields found: {report['total_schema_fields']}\nDocumented fields: {report['total_documented_fields']}")
    matching_count = len(set(report.get('matching_fields_list', [])) - {"classname", "enable", "disable", "forcecreate"})
    print(f"Matching/Implemented documented fields: {matching_count}\nDocumentation refinements applied: {report['refinements_applied']}\nDocumented structures added: {report['additions_made']}")
    if report['fields_in_code_not_in_doc']:
        print(f"\n⚠️  FIELDS IN CODE BUT NOT IN DOCUMENTATION ({len(report['fields_in_code_not_in_doc'])}):")
        print("\n".join(f"   - {field}" for field in report['fields_in_code_not_in_doc']))
    if report['fields_in_doc_not_in_code']:
        print(f"\n📚 FIELDS DOCUMENTED BUT NOT IMPLEMENTED ({len(report['fields_in_doc_not_in_code'])}):")
        print("\n".join(f"   - {field}" for field in report['fields_in_doc_not_in_code']))
    if report.get('matching_fields_list'):
        print(f"\n✅ IMPLEMENTED & DOCUMENTED FIELDS ({matching_count}):")
        matching_list = [f for f in report['matching_fields_list'] if f not in {"classname", "enable", "disable", "forcecreate"}]
        for field in matching_list[:10]: print(f"   - {field}")
        if len(matching_list) > 10: print(f"   ... and {len(matching_list) - 10} more")
    print(f"{'='*60}\n")

def generate_schema(classname: str, fields: Dict[str, Any]) -> Dict[str, Any]:
    final_schema = deepcopy(fields)
    final_schema["properties"] = {**{"classname": {"type": "string", "const": classname}, "forcecreate": {"type": "boolean"}, "enable": {"type": "boolean"}, "disable": {"type": "boolean"}}, **fields.get("properties", {})}
    final_required = {"classname"}.union(fields.get("required", []))
    final_schema["required"] = sorted(list(final_required))
    if 'oneOf' in fields:
        final_schema['oneOf'] = fields['oneOf']
    return final_schema

def extract_cpp_classname(code: str) -> Optional[str]:
    match = re.search(r'\bclass\s+(\w+)\s*[:{]', code)
    return match.group(1) if match else None

def determine_classname(code: str, path: Path) -> str:
    classname = extract_classname_from_constructor(code)
    if classname: return classname
    cpp_class = extract_cpp_classname(code)
    if cpp_class:
        return cpp_class
    header_path = path.with_suffix(".hpp")
    if not header_path.exists(): header_path = path.with_suffix(".h")
    if header_path.exists():
        try:
            header_class = extract_cpp_classname(header_path.read_text(encoding="utf-8", errors="ignore"))
            if header_class:
                return header_class
        except Exception as e:
            print(f"[ERROR] Failed to read header {header_path}: {e}")
    fallback = path.stem
    return fallback

def scan_for_schemas(src_dir: Path, out_dir: Path, doc_data: Dict[str, Any] = None) -> Tuple[Dict[str, Dict[str, Any]], Dict[str, Dict[str, Any]]]:
    all_schemas, all_reports = {}, {}
    for root, _, files in os.walk(src_dir):
        for file in files:
            if not file.endswith(".cpp"): continue
            path = Path(root, file)
            code = path.read_text(encoding="utf-8", errors="ignore")
            if not re.search(r'\bload\s*\(\s*const\s+Json::Value\s*&\s*\w+\s*\)', code): continue
            print(f"Processing: {path.name}")
            classname = determine_classname(code, path)
            schema, report = analyze_json_access_patterns(code, classname, doc_data)
            final_schema = generate_schema(classname, schema)
            all_schemas[classname] = final_schema
            all_reports[classname] = report
            (out_dir / f"{classname}.schema.json").write_text(json.dumps(final_schema, indent=2))
            print_detailed_comparison_report(report)
    return all_schemas, all_reports

def generate_summary_report(all_reports: Dict[str, Dict[str, Any]], out_dir: Path):
    print(f"\n{'='*80}\nCOMPREHENSIVE FIELD MISMATCH SUMMARY\n{'='*80}")
    summary = {"total_classes": len(all_reports), "classes_with_mismatches": 0, "total_code_only_fields": 0, "total_doc_only_fields": 0, "total_refinements": 0, "total_additions": 0}
    for report in all_reports.values():
        if report['fields_in_code_not_in_doc'] or report['fields_in_doc_not_in_code']: summary['classes_with_mismatches'] += 1
        summary['total_code_only_fields'] += len(report['fields_in_code_not_in_doc'])
        summary['total_doc_only_fields'] += len(report['fields_in_doc_not_in_code'])
        summary['total_refinements'] += report['refinements_applied']
        summary['total_additions'] += report.get('additions_made', 0)
    print("\n".join(f"{k.replace('_', ' ').title()}: {v}" for k, v in summary.items()))
    print(f"\n{'CLASSES WITH UNDOCUMENTED CODE FIELDS:':<50}\n" + "-" * 50)
    for classname, report in sorted(all_reports.items()):
        if report['fields_in_code_not_in_doc']: print(f"{classname}:\n" + "\n".join(f"  ❌ {f}" for f in report['fields_in_code_not_in_doc']))
    print(f"\n{'CLASSES WITH UNUSED DOCUMENTED FIELDS:':<50}\n" + "-" * 50)
    for classname, report in sorted(all_reports.items()):
        if report['fields_in_doc_not_in_code']: print(f"{classname}:\n" + "\n".join(f"  📝 {f}" for f in report['fields_in_doc_not_in_code']))
    (out_dir / "field_mismatch_report.json").write_text(json.dumps({"summary": summary, "detailed_reports": all_reports}, indent=2))
    print(f"\nDetailed report saved to: {out_dir / 'field_mismatch_report.json'}")

def get_cpp_variable_name_map() -> Dict[str, str]:
    return { "Reloadmode": "RELOAD_CLASS_SCHEMA", "Aev1": "AEV1_CLASS_SCHEMA", "Aev2": "AEV2_CLASS_SCHEMA", "Aehdr": "AEHDR_CLASS_SCHEMA", "AdaptiveAe": "ADAPTIVE_AE_CLASS_SCHEMA", "HDRAdaptiveAE": "HDRADAPTIVE_AE_CLASS_SCHEMA", "Awbv2": "AWBV2_CLASS_SCHEMA", "Afv1": "AFV1_CLASS_SCHEMA", "IspController": "ISP_CONTROLLER_CLASS_SCHEMA", "AutoHdr": "AUTO_HDR_SCHEMA", "DciHist": "DCI_HIST_SCHEMA", "SensorController": "SENSOR_CONTROLLER_CLASS_SCHEMA", "AGamma64": "AGAMMA64_CLASS_SCHEMA", "ACproc": "ACPROC_CLASS_SCHEMA", "ACprocPost": "ACPROC_POST_CLASS_SCHEMA", "Aeev1": "AEEV1_CLASS_SCHEMA", "ADmscv2": "ADMSCV2_CLASS_SCHEMA", "AWdrv4": "AWDRV4_CLASS_SCHEMA", "A3dnrv1": "A3DNRV1_CLASS_SCHEMA", "A2dnrv5": "A2DNRV5_CLASS_SCHEMA", "ADpf": "ADPF_CLASS_SCHEMA", "ABls": "ABLS_CLASS_SCHEMA", "ALsc": "ALSC_CLASS_SCHEMA" }

def format_classname_for_cpp(classname: str) -> str:
    return classname.upper() + "_CLASS_SCHEMA"

def generate_cpp_header(all_schemas: Dict[str, Any], out_dir: Path):
    header_path = out_dir / "aaa_config_schema.h"
    variable_map = get_cpp_variable_name_map()
    with open(header_path, "w") as f:
        f.write("#ifndef __AAA_CONFIG_SCHEMA_H__\n#define __AAA_CONFIG_SCHEMA_H__\n\n#include <map>\n#include <string>\n\n")
        
        f.write("""// Legacy schema for validating the original `root` array format
static constexpr const char* AAACONFIG_SCHEMA_TEXT = R"(
  {
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
      "root": {
        "type": "array",
        "items": {
          "type": "object",
          "properties": {
            "classname": { "type": "string" }
          },
          "required": [ "classname" ],
          "additionalProperties": true
        }
      }
    },
    "required": [ "root" ],
    "additionalProperties": true
  })";

""")
        
        for classname, schema in sorted(all_schemas.items()):
            schema_with_header = {"$schema": "http://json-schema.org/draft-07/schema#", **schema}
            variable_name = variable_map.get(classname, format_classname_for_cpp(classname))
            f.write(f'static constexpr const char* {variable_name} = R"(\n{json.dumps(schema_with_header, indent=2).replace(")", ") ")}\n)";\n\n')
            
        f.write("static std::map<std::string, const char*> classes_and_schema = {\n")
        for i, classname in enumerate(sorted(all_schemas.keys())):
            f.write(f'    {{"{classname}", {variable_map.get(classname, format_classname_for_cpp(classname))}}}{"," if i < len(all_schemas) - 1 else ""}\n')
        f.write("};\n\n#endif // __AAA_CONFIG_SCHEMA_H__\n")
    print(f"C++ schema header written to: {header_path}")

def transform_schema_for_iqsettings(all_schemas: Dict[str, Any]) -> Dict[str, Any]:
    class_properties = {}
    for classname, schema in all_schemas.items():
        transformed_schema = json.loads(json.dumps(schema))
        if "tables" in transformed_schema.get("properties", {}):
            tables_schema = transformed_schema["properties"]["tables"]
            if tables_schema.get("type") == "array" and isinstance(tables_schema.get("items"), dict):
                table_items_schema = tables_schema["items"]
                if "properties" in table_items_schema:
                    transformed_schema["properties"].update(table_items_schema.get("properties", {}))
                
                if "oneOf" in table_items_schema:
                    for sub_schema in table_items_schema["oneOf"]:
                        if "properties" in sub_schema:
                            transformed_schema["properties"].update(sub_schema["properties"])
                    transformed_schema["oneOf"] = table_items_schema["oneOf"]
                    
                if "required" in transformed_schema:
                    new_required = set(transformed_schema["required"])
                    new_required.discard("tables")
                    new_required.update(table_items_schema.get("required", []))
                    transformed_schema["required"] = sorted(list(new_required))
                del transformed_schema["properties"]["tables"]

        if "classname" in transformed_schema.get("properties", {}):
            del transformed_schema["properties"]["classname"]
            if "required" in transformed_schema and "classname" in transformed_schema["required"]:
                transformed_schema["required"].remove("classname")
                if not transformed_schema["required"]: del transformed_schema["required"]
        
        transformed_schema["additionalProperties"] = schema.get("additionalProperties", False)
        class_properties[classname] = transformed_schema
        
    return {"$schema": "http://json-schema.org/draft-07/schema#", "type": "object", "properties": {"automatic_algorithms": {"type": "object", "properties": class_properties, "additionalProperties": False}}, "required": ["automatic_algorithms"], "additionalProperties": True}

def remove_validation_and_description_from_schema(schema: Dict[str, Any]) -> Dict[str, Any]:
    keys_to_remove = {"minimum", "maximum", "enum", "const", "description"}
    if isinstance(schema, dict):
        return {k: remove_validation_and_description_from_schema(v) for k, v in schema.items() if k not in keys_to_remove}
    elif isinstance(schema, list):
        return [remove_validation_and_description_from_schema(item) for item in schema]
    return schema

def generate_cpp_library_header(all_schemas: Dict[str, Any], out_dir: Path):
    header_path = out_dir / "aaa_config_schema.hpp"
    iq_schema_ranges = transform_schema_for_iqsettings(all_schemas)
    iq_schema_no_validation = remove_validation_and_description_from_schema(iq_schema_ranges)
    with open(header_path, "w") as f:
        f.write("#ifndef __AAA_CONFIG_HPP__\n#define __AAA_CONFIG_HPP__\n\n#include <string_view>\n\n")
        indented_text = json.dumps(iq_schema_no_validation, indent=2).replace(')"', ') "')
        compact1 = re.sub(r':\s*{\n\s*("type":\s*(?:true|false|"[^"]*"))\n\s*}', r': { \1 }', indented_text)
        compact2 = re.sub(r':\s*{\n\s*"type":\s*"array",\n\s*"items":\s*({ "type":\s*"[^"]+"\s*})\n\s*}', r': { "type": "array", "items": \1 }', compact1)
        f.write(f'// Schema for basic structure validation of the new configuration format\nstatic constexpr std::string_view AUTOMATIC_ALGORITHMS_SCHEMA_TEXT = R"(\n{compact2}\n)";\n\n')
        f.write(f'// Schema for detailed range validation of the new configuration format\nstatic constexpr std::string_view AUTOMATIC_ALGORITHMS_SCHEMA_RANGES_TEXT = R"(\n{json.dumps(iq_schema_ranges, indent=2).replace(")", ") ")}\n)";\n')
        f.write("""
// Legacy schema for validating the original `root` array format
static constexpr std::string_view OLD_3ACONFIG_SCHEMA_TEXT = R"(
  {
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": { "root": { "type": "array", "items": { "type": "object", "properties": { "classname": { "type": "string" } }, "required": [ "classname" ], "additionalProperties": true } } },
    "required": [ "root" ], "additionalProperties": false
  })";
""")
        f.write("\n#endif // __AAA_CONFIG_HPP__\n")
    print(f"C++ library header written to: {header_path}")

def get_manual_schemas() -> Dict[str, Dict[str, Any]]:
    return {
        "Reloadmode": {"type": "object", "properties": {"classname": {"const": "Reloadmode"}, "mode": {"type": "string"}}, "required": ["classname", "mode"]},
        "IspController": {"type": "object", "properties": {"classname": {"const": "IspController"}, "suspend": {"type": "boolean"}, "enable": {"type": "boolean"}}},
        "SensorController": {"type": "object", "properties": {"classname": {"const": "SensorController"}, "suspend": {"type": "boolean"}, "enable": {"type": "boolean"}}, "required": ["classname"]},
        "AutoHdr": {"type": "object", "properties": {"classname": {"const": "AutoHdr"}, "enable": {"type": "boolean"}}, "required": ["classname", "enable"]},
        "Aev1": {"type": "object", "properties": {"classname": {"const": "Aev1"}, "enable": {"type": "boolean"}}, "required": ["classname", "enable"]},
        "Aehdr": {"type": "object", "properties": {"classname": {"const": "Aehdr"}, "enable": {"type": "boolean"}, "motionThreshold": {}, "roiWeight": {}, "motionFilter": {}, "maxSensorAgain": {}, "maxSensorDgain": {}, "maxIspDgain": {}}, "required": ["classname", "enable"]}
    }

def wrap_full_schema(all_schemas: Dict[str, Dict[str, Any]]) -> Dict[str, Any]:
    return {"$schema": "http://json-schema.org/draft-07/schema#", "type": "object", "properties": {"root": {"type": "array", "items": {"anyOf": list(all_schemas.values())}}}, "required": ["root"]}
    
def get_best_fit_error(error):
    if error.validator not in ["anyOf", "oneOf"] or not error.context:
        return error
    best_sub_error = max(error.context, key=lambda e: len(e.path))
    return get_best_fit_error(best_sub_error)

def format_validation_error(error, parent_path=deque()) -> str:
    if error.validator in ["anyOf", "oneOf"]:
        best_fit = get_best_fit_error(error)
        full_path = error.path + best_fit.path
        path_str = ".".join(str(p) for p in (parent_path + full_path))
        return f"At path {'.'.join(str(p) for p in (parent_path + error.path))}: Value '{error.instance}' is invalid. Most likely reason: At path '{path_str}', {best_fit.message}"

    path_str = ".".join(str(p) for p in (parent_path + error.path)) or "object root"
    if error.validator in ["minimum", "maximum"]:
        return f"At path '{path_str}': Value '{error.instance}' is invalid (must be {error.validator} {error.validator_value})."
    if error.validator == "type":
        return f"At path '{path_str}': Value '{error.instance}' has the wrong type (expected '{error.validator_value}')."
    return f"At path '{path_str}': {error.message}"

def normalize_error_path(path: deque) -> str:
    return ".".join(str(p) if not isinstance(p, int) else '[*]' for p in path) if path else "object root"

def validate_json_files(json_dir: Path, all_schemas: Dict[str, Dict[str, Any]], iq_schema: Dict[str, Any]) -> Tuple[Dict, Dict]:
    try:
        from jsonschema import Draft7Validator
    except ImportError:
        print("[ERROR] jsonschema package not installed. Run: pip install jsonschema")
        return {}, {}
        
    detailed_results = {}
    aggregated_errors = defaultdict(lambda: {
        "message": "", "count": 0, "files": set(), "invalid_values": defaultdict(int)
    })

    for root, _, files in os.walk(json_dir):
        for file in files:
            # MODIFICATION: Only check for .json extension, ignoring the name.
            if not file.endswith(".json"):
                continue
            
            path = Path(root, file)
            path_str = str(path)
            results = []
            detailed_results[path_str] = results
            
            try:
                data = json.loads(path.read_text(encoding="utf-8"))
            except Exception as e:
                results.append({"status": "FAIL", "classname": "File", "messages": [f"JSON parse error: {e}"]})
                continue

            # Check for and validate the new "automatic_algorithms" format
            if isinstance(data, dict) and "automatic_algorithms" in data:
                validator = Draft7Validator(iq_schema)
                errors = sorted(validator.iter_errors(data), key=lambda e: e.path)
                if errors:
                    results.append({"status": "FAIL", "classname": "New Format", "messages": [format_validation_error(e) for e in errors]})
                    for error in errors:
                        best_fit = get_best_fit_error(error)
                        if best_fit.validator == "required": continue
                        
                        classname = best_fit.path[1] if len(best_fit.path) > 1 else "Unknown"
                        aggregation_path = normalize_error_path(best_fit.path)
                        error_key = (classname, aggregation_path, best_fit.validator, best_fit.validator_value)
                        
                        agg_entry = aggregated_errors[error_key]
                        if not agg_entry["message"]:
                           agg_entry["message"] = f"Value must be {best_fit.validator} {best_fit.validator_value}"
                        
                        agg_entry["count"] += 1
                        agg_entry["files"].add(path_str)
                        agg_entry["invalid_values"][str(best_fit.instance)] +=1
                else:
                    results.append({"status": "OK", "classname": "New Format"})

            # Check for and validate the old "root" array format
            elif isinstance(data, dict) and "root" in data and isinstance(data["root"], list):
                for idx, entry in enumerate(data["root"]):
                    clsname = entry.get("classname")
                    if not clsname:
                        results.append({"status": "WARN", "item_index": idx, "classname": "Unknown", "messages": ["Missing 'classname'"]}); continue
                    
                    schema = all_schemas.get(clsname)
                    if not schema:
                        results.append({"status": "WARN", "item_index": idx, "classname": clsname, "messages": ["Unknown classname"]}); continue
                        
                    validator = Draft7Validator(schema)
                    errors = sorted(validator.iter_errors(entry), key=lambda e: e.path)
                    
                    if errors:
                        results.append({"status": "FAIL", "item_index": idx, "classname": clsname, "messages": [format_validation_error(e) for e in errors]})
                        for top_level_error in errors:
                            best_fit_error = get_best_fit_error(top_level_error)
                            if best_fit_error.validator == "required": continue
                            aggregation_path = normalize_error_path(top_level_error.path)
                            error_key = (clsname, aggregation_path, best_fit_error.validator, best_fit_error.validator_value)
                            
                            agg_entry = aggregated_errors[error_key]
                            if not agg_entry["message"]:
                               agg_entry["message"] = f"Value must be {best_fit_error.validator} {best_fit_error.validator_value}"
                            
                            agg_entry["count"] += 1
                            agg_entry["files"].add(path_str)
                            agg_entry["invalid_values"][str(best_fit_error.instance)] += 1
                    else:
                        extra = set(entry.keys()) - set(validator.schema.get("properties", {}).keys())
                        if extra: results.append({"status": "WARN", "item_index": idx, "classname": clsname, "messages": [f"Unexpected fields: {sorted(extra)}"]})
                        else: results.append({"status": "OK", "item_index": idx, "classname": clsname})
            else:
                 results.append({"status": "FAIL", "classname": "File", "messages": ["File is not in a recognized format (neither 'root' nor 'automatic_algorithms' found)."]})

    return detailed_results, dict(aggregated_errors)

def print_validation_report(all_results: Dict):
    if not all_results: print("No configuration files found to validate."); return
    print(f"\n{'='*80}\nDETAILED VALIDATION REPORT\n{'='*80}")
    for file_path, results in sorted(all_results.items()):
        print(f"\n📄 Validating file: {file_path}\n" + "-" * (len(file_path) + 18))
        if not results: print("  File was empty or could not be parsed."); continue
        has_issues = any(r['status'] != 'OK' for r in results)
        for result in results:
            idx = result.get('item_index')
            item_id = f"Item {idx}" if idx is not None else "File-level"
            cls, status = result.get('classname', 'N/A'), result['status']

            if status == "FAIL":
                print(f"  ❌ [FAIL] {item_id} ({cls})\n" + "\n".join(f"     - {line}" for msg in result['messages'] for line in msg.split('\n')))
            elif status == "WARN":
                print(f"  ⚠️  [WARN] {item_id} ({cls})\n" + "\n".join(f"     - {msg}" for msg in result['messages']))
        if not has_issues: print("  ✅ [OK] All items passed validation.")
    print(f"\n{'='*80}")

def print_aggregated_validation_summary(aggregated_errors: Dict):
    if not aggregated_errors:
        print("\nNo aggregatable validation errors found in any files.")
        return
        
    print(f"\n{'='*80}\nAGGREGATED VALIDATION SUMMARY\n{'='*80}")
    print(f"Found {len(aggregated_errors)} unique types of validation errors across all files.")

    sorted_errors = sorted(aggregated_errors.items(), key=lambda item: item[1]['count'], reverse=True)

    for (classname, path, _, _), details in sorted_errors:
        print(f"\n- CLASS: {classname} | PATH: {path}")
        print(f"  - Error: {details['message']}")
        print(f"  - Total Occurrences: {details['count']}")
        
        invalid_vals_str = ", ".join(f"'{v}' ({c} times)" for v, c in details['invalid_values'].items())
        print(f"  - Invalid Values Seen: {invalid_vals_str}")

        affected_files = sorted(list(details['files']))
        files_to_show = 5
        print(f"  - Affected Files ({len(affected_files)}):")
        for f in affected_files[:files_to_show]:
            print(f"    - {f}")
        if len(affected_files) > files_to_show:
            print(f"    ... and {len(affected_files) - files_to_show} more.")
    print(f"\n{'='*80}")

def main():
    parser = argparse.ArgumentParser(description="Generate JSON schemas from C++ source code and documentation.")
    parser.add_argument("--root", type=str, required=True, help="Root directory of C++ source files.")
    parser.add_argument("--out", type=str, required=True, help="Directory to output generated schema files.")
    parser.add_argument("--doc", type=str, help="Path to the documentation JSON file.")
    parser.add_argument("--validate", type=str, help="Optional directory of config files to validate against the generated schemas.")
    args = parser.parse_args()
    
    src_dir, out_dir = Path(args.root), Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)
    
    doc_data = load_documentation_json(Path(args.doc)) if args.doc else {}
    
    schemas, reports = scan_for_schemas(src_dir, out_dir, doc_data)
    all_schemas = {**schemas, **get_manual_schemas()}
    
    if reports:
        generate_summary_report(reports, out_dir)
        
    full_schema = wrap_full_schema(all_schemas)
    (out_dir / "full_schema.json").write_text(json.dumps(full_schema, indent=2))
    print(f"\nFull schema for 'root' format written to: {out_dir / 'full_schema.json'}")
    
    # Generate and write the schema for the new format
    iq_schema = transform_schema_for_iqsettings(all_schemas)
    (out_dir / "automatic_algorithms_schema.json").write_text(json.dumps(iq_schema, indent=2))
    print(f"Schema for 'automatic_algorithms' format written to: {out_dir / 'automatic_algorithms_schema.json'}")
    
    generate_cpp_header(all_schemas, out_dir)
    generate_cpp_library_header(all_schemas, out_dir)
    
    if args.validate:
        print(f"\nValidating JSON files in: {args.validate}")
        detailed_results, aggregated_errors = validate_json_files(Path(args.validate), all_schemas, iq_schema)
        print_aggregated_validation_summary(aggregated_errors)
        print_validation_report(detailed_results)

if __name__ == "__main__":
    main()