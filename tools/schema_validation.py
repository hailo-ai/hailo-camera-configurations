import argparse
import json
import sys
from pathlib import Path
import jsonschema
import fnmatch
from collections import defaultdict # --- NEW: Import defaultdict for easier counting ---


# --- All functions from the previous version remain unchanged ---
def strip_internal_ids(schema_node):
    if isinstance(schema_node, dict):
        for key, value in list(schema_node.items()):
            if key == 'definitions':
                if isinstance(value, dict):
                    for def_name, def_content in value.items():
                        if isinstance(def_content, dict) and '$id' in def_content:
                            del def_content['$id']
                        strip_internal_ids(def_content)
            else:
                strip_internal_ids(value)
    elif isinstance(schema_node, list):
        for item in schema_node:
            strip_internal_ids(item)

def load_mapping_rules(filepath: Path) -> dict[str, str]:
    if not filepath.is_file():
        print(f"Error: Mapping file not found at '{filepath}'")
        sys.exit(1)
    rules = {}
    with open(filepath, 'r', encoding='utf-8') as f:
        for i, line in enumerate(f, 1):
            clean_line = line.strip()
            if not clean_line or clean_line.startswith('#'): continue
            if ':' not in clean_line:
                print(f"Warning: Malformed rule on line {i} in '{filepath}'. Skipping.")
                continue
            config_file, schema_file = [part.strip() for part in clean_line.split(':', 1)]
            rules[config_file] = schema_file
    print(f"Successfully loaded {len(rules)} mapping rules from '{filepath.name}'.")
    return rules

def load_schemas(schema_dir: Path) -> dict[str, dict]:
    if not schema_dir.is_dir():
        print(f"Error: Schema directory not found at '{schema_dir}'")
        sys.exit(1)
    schemas = {}
    for schema_path in schema_dir.glob('*.json'):
        try:
            with open(schema_path, 'r', encoding='utf-8') as f:
                content = json.load(f)
            if len(content.keys()) == 1 and next(iter(content)) in ["allOf", "oneOf", "anyOf"]:
                keyword = next(iter(content))
                content = {"$schema": "http://json-schema.org/draft-07/schema#", "$id": schema_path.name, keyword: content[keyword]}
            else:
                if "$id" not in content: content["$id"] = schema_path.name
            strip_internal_ids(content)
            schemas[schema_path.name] = content
        except Exception as e:
            print(f"Warning: Could not read or process schema file '{schema_path.name}': {e}. Skipping.")
    print(f"Successfully loaded, normalized, and cleaned {len(schemas)} schemas from '{schema_dir.name}'.")
    return schemas


def main():
    parser = argparse.ArgumentParser(
        description="Validate a directory of JSON configuration files against a set of JSON schemas.",
        formatter_class=argparse.RawTextHelpFormatter
    )
    parser.add_argument("schema_dir", type=Path, help="Path to the directory containing JSON schema files.")
    parser.add_argument("config_dir", type=Path, help="Path to the root directory of configurations to validate.")
    parser.add_argument("mapping_file", type=Path, help="Path to the text file that maps config filenames to schema filenames.")
    args = parser.parse_args()

    mapping_rules = load_mapping_rules(args.mapping_file)
    schemas = load_schemas(args.schema_dir)

    if not mapping_rules or not schemas:
        print("Error: No rules or schemas loaded. Exiting.")
        sys.exit(1)

    exact_mapping_rules = {}
    wildcard_mapping_rules = {}
    for pattern, schema in mapping_rules.items():
        if '*' in pattern or '?' in pattern or '[' in pattern:
            wildcard_mapping_rules[pattern] = schema
        else:
            exact_mapping_rules[pattern] = schema
            
    try:
        print("Successfully created a schema registry for resolving references.")
    except Exception as e:
        print(f"Error: Could not create the schema registry. There might be an invalid schema. Error: {e}")
        sys.exit(1)

    print("\n--- Starting Validation ---")
    
    config_files_to_check = list(args.config_dir.rglob('*.json'))
    if not config_files_to_check:
        print(f"No '.json' files found in '{args.config_dir}'. Nothing to do.")
        return
        
    passed_count = 0
    failed_files = {} # --- MODIFICATION: Store detailed error info for failed files ---
    grand_total_errors_by_category = defaultdict(int) # --- NEW: For the final summary ---


    for config_path in config_files_to_check:
        config_filename = config_path.name
        schema_filename = None

        if config_filename in exact_mapping_rules:
            schema_filename = exact_mapping_rules[config_filename]
        else:
            for pattern, schema in wildcard_mapping_rules.items():
                if fnmatch.fnmatch(config_filename, pattern):
                    schema_filename = schema
                    break
        
        if schema_filename is None:
            continue

        if schema_filename not in schemas:
            print(f"\n[SKIPPED] {config_path.relative_to(args.config_dir.parent)} (Schema '{schema_filename}' not found)")
            continue
        
        schema_obj = schemas[schema_filename]
        
        try:
            with open(config_path, 'r', encoding='utf-8') as f:
                config_data = json.load(f)
        except Exception as e:
            # --- MODIFICATION: Store file-level errors for reporting ---
            failed_files[config_path] = {"file_error": f"Could not read or parse JSON: {e}"}
            continue

        try:
            validator = jsonschema.Draft7Validator(schema_obj)
            errors = sorted(validator.iter_errors(config_data), key=lambda e: e.path)
            
            if not errors:
                print(f"[PASSED] {config_path.relative_to(args.config_dir.parent)}")
                passed_count += 1
            else:
                # --- MODIFICATION: Collect and categorize errors instead of printing immediately ---
                print(f"[FAILED] {config_path.relative_to(args.config_dir.parent)}")
                errors_by_category = defaultdict(list)
                for error in errors:
                    # Categorize by the validator keyword (e.g., 'required', 'type', 'pattern')
                    errors_by_category[error.validator].append(error)
                    grand_total_errors_by_category[error.validator] += 1
                
                failed_files[config_path] = errors_by_category

        except jsonschema.SchemaError as e:
            failed_files[config_path] = {"schema_error": f"The schema '{schema_filename}' is invalid: {e.message}"}

    # --- NEW SECTION: Hierarchical Error Reporting ---
    print("\n\n--- Validation Error Report ---")
    if not failed_files:
        print("No validation errors found.")
    else:
        for file_path, errors in failed_files.items():
            print("\n" + "="*80)
            print(f"FILE: {file_path.relative_to(args.config_dir.parent)}")
            print("="*80)

            if "file_error" in errors:
                print(f"  FATAL: {errors['file_error']}")
                continue
            if "schema_error" in errors:
                print(f"  FATAL: {errors['schema_error']}")
                continue

            # Print the summary for the file
            print("  Error Summary:")
            for category, error_list in errors.items():
                # Critical errors are typically structural
                criticality = "CRITICAL" if category in ['required', 'additionalProperties', 'type'] else "Warning"
                print(f"    - {category.upper()} ({criticality}): {len(error_list)} error(s)")
            
            # Print the detailed errors, grouped by category
            for category, error_list in errors.items():
                print(f"\n  --- Details for '{category.upper()}' ---")
                for error in error_list:
                    error_path = "$." + ".".join(map(str, error.path)) if error.path else "$"
                    print(f"    - Path:    {error_path}")
                    print(f"      Message: {error.message}")
    
    # --- NEW SECTION: Final Grand Summary ---
    print("\n\n" + "#"*80)
    print("### Overall Validation Summary")
    print("#"*80)
    total_checked = len(config_files_to_check)
    total_failed = len(failed_files)
    print(f"Total Files Checked: {total_checked}")
    print(f"  - Passed: {passed_count}")
    print(f"  - Failed: {total_failed}")

    if total_failed > 0:
        print("\n--- Grand Total of Errors by Category ---")
        # Sort by count for better readability
        sorted_error_counts = sorted(grand_total_errors_by_category.items(), key=lambda item: item[1], reverse=True)
        for category, count in sorted_error_counts:
            criticality = "CRITICAL" if category in ['required', 'additionalProperties', 'type'] else "Warning"
            print(f"  - {category.upper()} ({criticality}): {count} total error(s)")
    print("#"*80)


    if total_failed > 0:
        sys.exit(1)

if __name__ == "__main__":
    main()