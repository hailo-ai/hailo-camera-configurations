import os
import json
import re
import sys
from pathlib import Path
from copy import deepcopy
import shutil
import argparse
from typing import Any

class ConfigProcessor:
    """
    Processes a configuration directory, recursively filling in default values
    in JSON files based on schemas extracted from a C++ header. It handles
    nested JSON files linked by absolute Linux-style paths.
    """
    def __init__(self, input_dir: Path, output_dir: Path, header_path: Path, linux_root: str):
        self.input_dir = input_dir.resolve()
        self.output_dir = output_dir.resolve()
        self.header_path = header_path
        self.linux_root = linux_root
        self.processed_internal_paths = set()
        
        print("--- Initializing Config Processor ---")
        self.schemas = self._extract_json_schemas_from_header()
        self.flat_schemas = self._flatten_schemas()
        
        if not self.schemas:
            print("❌ ERROR: No schemas were successfully loaded. Please check the header file and parsing errors.", file=sys.stderr)
            sys.exit(1)
            
        print(f"✅ Loaded {len(self.schemas)} schemas from header.")

    def _sanitize_json_string(self, raw_json: str) -> str:
        """A robust function to clean a JSON string extracted from C++ source code."""
        # Remove C-style block comments (/* ... */)
        text = re.sub(r'/\*.*?\*/', '', raw_json, flags=re.DOTALL)

        cleaned_lines = []
        # Process line-by-line to correctly handle line comments without breaking URLs
        for line in text.splitlines():
            in_string = False
            comment_start_index = -1
            for i, char in enumerate(line):
                if char == '"':
                    in_string = not in_string
                elif char == '/' and i + 1 < len(line) and line[i+1] == '/' and not in_string:
                    comment_start_index = i
                    break
            if comment_start_index != -1:
                line = line[:comment_start_index]
            cleaned_lines.append(line)

        # Join, remove trailing commas, and return
        full_sanitized_text = " ".join(cleaned_lines)
        full_sanitized_text = re.sub(r',\s*([}\]])', r'\1', full_sanitized_text)
        return full_sanitized_text.strip()

    def _extract_json_schemas_from_header(self) -> dict:
        try:
            header_content = self.header_path.read_text(encoding='utf-8')
        except FileNotFoundError:
            print(f"❌ ERROR: Header file not found at {self.header_path}", file=sys.stderr)
            sys.exit(1)
            
        schema_pattern = re.compile(r'static\s+const\s+nlohmann::json\s+(\w+)\s*=\s*R"\((.*?)\)"_json;', re.DOTALL)
        schemas = {}
        for match in schema_pattern.finditer(header_content):
            name, raw_json_from_cpp = match.groups()
            try:
                sanitized_json = self._sanitize_json_string(raw_json_from_cpp)
                if not sanitized_json:
                    continue
                schemas[name] = json.loads(sanitized_json)
            except json.JSONDecodeError as e:
                print(f"⚠️  Could not parse schema '{name}' after sanitization: {e}")
        return schemas

    def _flatten_schemas(self) -> dict:
        """Creates a map of all top-level property names to their schemas for quick lookup."""
        flat = {}
        for schema in self.schemas.values():
            for prop_name, prop_schema in schema.get("properties", {}).items():
                flat[prop_name] = prop_schema
        return flat

    def _get_default_value(self, schema: dict):
        """Recursively generates a default value for a given schema."""
        if 'default' in schema:
            return schema['default']
        schema_type = schema.get('type')
        if schema_type == 'object':
            return { k: self._get_default_value(v) for k, v in schema.get('properties', {}).items() }
        elif schema_type == 'array':
            item_schema = schema.get('items', {})
            # Only create a default item if the items are supposed to be objects
            return [self._get_default_value(item_schema)] if item_schema and 'properties' in item_schema else []
        elif schema_type == 'string':
            return schema.get('enum', [""])[0]
        elif schema_type == 'integer':
            return 0
        elif schema_type == 'boolean':
            return False
        elif schema_type == 'number':
            return 0.0
        return None

    def run(self):
        """Starts the process of traversing and processing the configuration directory."""
        print(f"\n--- Starting processing of directory: {self.input_dir} ---")
        if not self.input_dir.is_dir():
            print(f"❌ ERROR: Input directory not found.", file=sys.stderr)
            return

        for root, _, files in os.walk(self.input_dir):
            for file in files:
                full_input_path = Path(root) / file
                relative_path = full_input_path.relative_to(self.input_dir)
                full_output_path = self.output_dir / relative_path
                full_output_path.parent.mkdir(parents=True, exist_ok=True)

                if full_input_path.suffix == ".json":
                    print(f"\nProcessing file: {full_input_path}")
                    self._process_file_path(full_input_path, full_output_path)
                else:
                    shutil.copy2(full_input_path, full_output_path)
        print("\n--- Processing complete. ---")
        
    def _process_file_path(self, input_path: Path, output_path: Path):
        """Handles the processing of a single JSON file."""
        try:
            data = json.loads(input_path.read_text(encoding='utf-8'))
            original_data_str = json.dumps(data, sort_keys=True)
            
            modified_data = self._process_json_content(data)
            
            modified_data_str = json.dumps(modified_data, sort_keys=True)
            
            if modified_data_str != original_data_str:
                output_path.write_text(json.dumps(modified_data, indent=2), encoding='utf-8')
                print(f"✅ Changes applied. Saved to: {output_path}")
            else:
                shutil.copy2(input_path, output_path)
                print(f"ℹ️  No changes needed for {input_path.name}.")
        except Exception as e:
            print(f"❌ ERROR processing {input_path}: {e}", file=sys.stderr)
            shutil.copy2(input_path, output_path)

    def _process_json_content(self, data: dict) -> dict:
        """Processes a JSON object by applying schemas to its top-level keys."""
        modified_data = deepcopy(data)
        for key in modified_data:
            schema_for_key = self.flat_schemas.get(key)
            if schema_for_key:
                print(f"  - Found schema for key '{key}'. Applying defaults...")
                modified_data[key] = self._fill_recursive(modified_data[key], schema_for_key)
        return modified_data

    def _fill_recursive(self, data: Any, schema: dict) -> Any:
        """The core recursive worker that traverses the data and fills in defaults."""
        if isinstance(data, str) and data.startswith(self.linux_root) and data.endswith(".json"):
            # Translate the absolute linux path to a local file path
            relative_path_str = data[len(self.linux_root):].lstrip("/")
            local_input_path = self.input_dir / relative_path_str.replace("/", os.path.sep)
            
            if not local_input_path.is_file():
                print(f"⚠️  Path reference '{data}' resolved to a non-existent local file: {local_input_path}")
                return data

            canonical_path = str(local_input_path.resolve())
            if canonical_path in self.processed_internal_paths:
                return data
            
            print(f"📂 Found linked JSON: '{data}'. Processing sub-file...")
            self.processed_internal_paths.add(canonical_path)
            
            relative_to_input_root = local_input_path.relative_to(self.input_dir)
            local_output_path = self.output_dir / relative_to_input_root
            
            try:
                # Process the linked file as a new, independent unit
                self._process_file_path(local_input_path, local_output_path)
            except Exception as e:
                print(f"❌ ERROR processing linked file {local_input_path}: {e}", file=sys.stderr)
                shutil.copy2(local_input_path, local_output_path)
            return data

        elif isinstance(data, list):
            item_schema = schema.get("items", {})
            # If the data is an empty list AND the schema says its items should be objects, populate it.
            if not data and item_schema and item_schema.get("type") == "object":
                print(f"  🟢 Populating empty array with one default object item.")
                return [self._get_default_value(item_schema)]
            
            return [self._fill_recursive(item, item_schema) for item in data]

        elif isinstance(data, dict):
            result = deepcopy(data)
            # Add defaults for properties defined in the schema but missing from the data
            if schema and schema.get("type") == "object":
                for key, prop_schema in schema.get("properties", {}).items():
                    if key not in result:
                        default = self._get_default_value(prop_schema)
                        result[key] = default
                        print(f"  🟢 Added missing field '{key}' with default: {json.dumps(default)}")
            
            # Recurse into all existing values to handle nested structures and paths
            for key, value in result.items():
                prop_schema = schema.get("properties", {}).get(key, {})
                result[key] = self._fill_recursive(value, prop_schema)
            return result
            
        return data

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Fill default values in a configuration tree based on JSON schemas from a header.")
    parser.add_argument("--input_dir", required=True, help="Directory containing the source configuration files.")
    parser.add_argument("--output_dir", required=True, help="Directory where the processed configuration will be saved.")
    parser.add_argument("--header_file", required=True, help="Path to the C++ header file containing the nlohmann::json schemas.")
    parser.add_argument("--linux_root", required=True, help="The absolute path prefix used in the JSON files (e.g., '/etc/imaging/cfg/').")
    args = parser.parse_args()
    
    processor = ConfigProcessor(
        input_dir=Path(args.input_dir),
        output_dir=Path(args.output_dir),
        header_path=Path(args.header_file),
        linux_root=args.linux_root
    )
    processor.run()