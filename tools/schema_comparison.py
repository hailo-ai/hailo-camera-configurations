import json
import re
import argparse
from pathlib import Path
from typing import Dict, Any, Optional

def extract_schema_from_hpp(content: str) -> Optional[Dict[str, Any]]:
    """
    Extracts the schema from a .hpp file's raw string literal.
    It prioritizes the schema with ranges and falls back to the one without.
    """
    schema_name_with_ranges = "AUTOMATIC_ALGORITHMS_SCHEMA_RANGES_TEXT"
    schema_name_no_ranges = "AUTOMATIC_ALGORITHMS_SCHEMA_TEXT"

    # 1. Prioritize the schema with ranges
    pattern_with_ranges = re.compile(
        rf'static constexpr std::string_view\s+{schema_name_with_ranges}\s*=\s*R"\((.*?)\)";',
        re.DOTALL
    )
    match = pattern_with_ranges.search(content)
    
    if match:
        try:
            print(f"[INFO] Found preferred schema '{schema_name_with_ranges}' in .hpp file.")
            return json.loads(match.group(1))
        except json.JSONDecodeError as e:
            print(f"[ERROR] Failed to parse JSON from '{schema_name_with_ranges}': {e}")
            return None

    # 2. Fallback to the schema without ranges if the first one wasn't found
    pattern_no_ranges = re.compile(
        rf'static constexpr std::string_view\s+{schema_name_no_ranges}\s*=\s*R"\((.*?)\)";',
        re.DOTALL
    )
    match = pattern_no_ranges.search(content)

    if match:
        try:
            print(f"[WARN] Could not find '{schema_name_with_ranges}'. Falling back to '{schema_name_no_ranges}'.")
            return json.loads(match.group(1))
        except json.JSONDecodeError as e:
            print(f"[ERROR] Failed to parse JSON from fallback schema '{schema_name_no_ranges}': {e}")
            return None
    
    # 3. If neither is found, fail
    print(f"[ERROR] Could not find '{schema_name_with_ranges}' or '{schema_name_no_ranges}' in the .hpp file.")
    return None

def extract_schemas_from_h(content: str) -> Optional[Dict[str, Any]]:
    """Extracts and combines individual schemas from a .h file."""
    schemas = {}
    pattern = re.compile(r'static constexpr const char\*\s+(\w+_CLASS_SCHEMA)\s*=\s*R"\((.*?)\)";', re.DOTALL)
    
    for match in pattern.finditer(content):
        schema_str = match.group(2)
        try:
            schema_json = json.loads(schema_str)
            classname = schema_json.get("properties", {}).get("classname", {}).get("const")
            if classname:
                schemas[classname] = schema_json
        except json.JSONDecodeError:
            print(f"[WARN] Could not parse schema block for {match.group(1)}")
            continue
    
    if not schemas:
        print("[ERROR] Could not find any valid schema blocks in the .h file.")
        return None
        
    # Re-wrap into the expected "root" array structure for comparison
    return {
        "$schema": "http://json-schema.org/draft-07/schema#",
        "type": "object",
        "properties": { "root": { "type": "array", "items": { "anyOf": list(schemas.values()) } } },
        "required": ["root"]
    }


def load_schema_from_file(file_path: Path) -> Optional[Dict[str, Any]]:
    """Loads a schema from a file, auto-detecting the type (.json, .h, .hpp)."""
    print(f"[INFO] Loading schema from: {file_path}")
    if not file_path.exists():
        print(f"[ERROR] File not found: {file_path}")
        return None

    content = file_path.read_text(encoding='utf-8')
    
    if file_path.suffix == '.json':
        try:
            return json.loads(content)
        except json.JSONDecodeError as e:
            print(f"[ERROR] Invalid JSON in {file_path}: {e}")
            return None
    elif file_path.suffix == '.hpp':
        return extract_schema_from_hpp(content)
    elif file_path.suffix == '.h':
        print("[WARN] Comparing .h files is for legacy schemas and may not be what you want.")
        return extract_schemas_from_h(content)
    else:
        print(f"[ERROR] Unsupported file type: {file_path.suffix}")
        return None

def compare_field_properties(old_prop: Dict, new_prop: Dict) -> list:
    """Compares individual properties of a field (type, validation rules)."""
    changes = []
    attrs_to_check = ['type', 'minimum', 'maximum', 'minItems', 'maxItems', 'enum']
    
    for attr in attrs_to_check:
        old_val = old_prop.get(attr)
        new_val = new_prop.get(attr)
        if old_val != new_val:
            changes.append(f"'{attr}' changed from {json.dumps(old_val)} to {json.dumps(new_val)}")
            
    # Compare array item types
    old_items = old_prop.get('items', {})
    new_items = new_prop.get('items', {})
    if isinstance(old_items, dict) and isinstance(new_items, dict):
        old_item_type = old_items.get('type')
        new_item_type = new_items.get('type')
        if old_item_type != new_item_type:
            changes.append(f"'items.type' changed from {json.dumps(old_item_type)} to {json.dumps(new_item_type)}")

    return changes

def compare_class_block(old_block: Dict, new_block: Dict) -> Dict:
    """Compares the schemas for a single class."""
    diff = {'added': [], 'removed': [], 'changed': {}, 'required_changes': {}}
    
    old_fields = set(old_block.get('properties', {}).keys())
    new_fields = set(new_block.get('properties', {}).keys())
    
    diff['added'] = sorted(list(new_fields - old_fields))
    diff['removed'] = sorted(list(old_fields - new_fields))
    
    # Check for changes in common fields
    for field in sorted(list(old_fields & new_fields)):
        old_prop = old_block['properties'][field]
        new_prop = new_block['properties'][field]
        
        field_changes = compare_field_properties(old_prop, new_prop)
        if field_changes:
            diff['changed'][field] = field_changes

    # Check for changes in the 'required' list
    old_req = set(old_block.get('required', []))
    new_req = set(new_block.get('required', []))
    
    if old_req != new_req:
        diff['required_changes']['added'] = sorted(list(new_req - old_req))
        diff['required_changes']['removed'] = sorted(list(old_req - new_req))

    return diff

def print_report(report: Dict):
    """Prints the comparison report in a human-readable format."""
    print("\n" + "="*80)
    print("SCHEMA COMPARISON REPORT")
    print("="*80)
    
    has_changes = False

    if report.get('added_classes') or report.get('removed_classes'):
        has_changes = True
        if report['added_classes']:
            print("\n[+] Classes Added:")
            for c in report['added_classes']:
                print(f"  - {c}")
        if report['removed_classes']:
            print("\n[-] Classes Removed:")
            for c in report['removed_classes']:
                print(f"  - {c}")
    
    print("\n[~] Changes within Common Classes:")
    changes_in_common = False
    for classname, diff in sorted(report.get('class_diffs', {}).items()):
        is_class_changed = diff['added'] or diff['removed'] or diff['changed'] or diff['required_changes']
        if is_class_changed:
            has_changes = True
            changes_in_common = True
            print(f"\n--- {classname} ---")
            if diff['added']:
                for field in diff['added']:
                    print(f"  + Added field: '{field}'")
            if diff['removed']:
                for field in diff['removed']:
                    print(f"  - Removed field: '{field}'")
            if diff['changed']:
                for field, changes in diff['changed'].items():
                    print(f"  ! Changed field: '{field}'")
                    for change in changes:
                        print(f"    - {change}")
            if diff['required_changes'].get('added') or diff['required_changes'].get('removed'):
                 print(f"  ! Changed 'required' status:")
                 for field in diff['required_changes']['added']:
                     print(f"    - '{field}' is now REQUIRED")
                 for field in diff['required_changes']['removed']:
                     print(f"    - '{field}' is NO LONGER REQUIRED")
    
    if not changes_in_common:
        print("  No differences found in common classes.")

    if not has_changes:
        print("\n✅ No functional differences found between the two schemas.")
    
    print("\n" + "="*80)

def main():
    parser = argparse.ArgumentParser(description="Compare two JSON schema files (.json, .h, or .hpp) and report the differences.")
    parser.add_argument("--old", required=True, type=Path, help="Path to the old schema file.")
    parser.add_argument("--new", required=True, type=Path, help="Path to the new schema file.")
    args = parser.parse_args()

    old_schema = load_schema_from_file(args.old)
    new_schema = load_schema_from_file(args.new)

    if not old_schema or not new_schema:
        print("\n[FATAL] Cannot proceed with comparison due to loading errors.")
        return

    # Normalize the structure for comparison
    try:
        old_classes = old_schema['properties']['automatic_algorithms']['properties']
        new_classes = new_schema['properties']['automatic_algorithms']['properties']
    except KeyError:
        print("[FATAL] Could not find the expected structure 'properties.automatic_algorithms.properties' in one of the schemas.")
        print("[INFO] This script is designed to compare schemas generated by the new script format.")
        return

    old_class_set = set(old_classes.keys())
    new_class_set = set(new_classes.keys())
    
    report = {
        "added_classes": sorted(list(new_class_set - old_class_set)),
        "removed_classes": sorted(list(old_class_set - new_class_set)),
        "class_diffs": {}
    }

    common_classes = old_class_set & new_class_set
    for classname in sorted(list(common_classes)):
        diff = compare_class_block(old_classes[classname], new_classes[classname])
        report["class_diffs"][classname] = diff
        
    print_report(report)

if __name__ == "__main__":
    main()