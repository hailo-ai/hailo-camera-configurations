import os
import re
import json
import argparse
from pathlib import Path

# Regex patterns (unchanged)
RE_LITERAL_SCHEMA = re.compile(
    r'static\s+(?:const\s+)?nlohmann::json\s+([a-zA-Z0-9_]+)\s*=\s*R"\((.*?)\)"_json;',
    re.DOTALL
)
RE_COMPOUND_SCHEMA = re.compile(
    r'static\s+(?:const\s+)?nlohmann::json\s+([a-zA-Z0-9_]+)\s*=\s*\{\s*\{"(allOf|oneOf|anyOf)",\s*\{(.*?)\}\}\s*\};',
    re.DOTALL
)
RE_CONDITIONAL_SCHEMA = re.compile(
    r'static\s+nlohmann::json\s+([a-zA-Z0-9_]+)\s*=\s*is_env_variable_on\((.*?)\)\s*\?'
    r'\s*nlohmann::json::parse\((.*?)\)\s*:'
    r'\s*nlohmann::json::parse\((.*?)\);',
    re.DOTALL
)
RE_EXTERNAL_TEXT_SCHEMA = re.compile(
    r'static\s+constexpr\s+std::string_view\s+([a-zA-Z0-9_]+)\s*=\s*R"\((.*?)\)";',
    re.DOTALL
)


class SchemaParser:
    """Parses C++ header files to extract raw schema definitions."""
    def __init__(self):
        self.schemas = {}

    def parse_file(self, content, file_type):
        if file_type == 'secondary':
            for match in RE_EXTERNAL_TEXT_SCHEMA.finditer(content):
                name, schema_str = match.groups()
                self.schemas[name.strip()] = {'type': 'external_text', 'content': schema_str.strip()}
        elif file_type == 'primary':
            for match in RE_LITERAL_SCHEMA.finditer(content):
                name, schema_str = match.groups()
                self.schemas[name.strip()] = {'type': 'literal', 'content': schema_str.strip()}
            for match in RE_COMPOUND_SCHEMA.finditer(content):
                name, keyword, components_str = match.groups()
                components = [c.strip() for c in components_str.split(',') if c.strip()]
                self.schemas[name.strip()] = {'type': 'compound', 'keyword': keyword, 'components': components}
            for match in RE_CONDITIONAL_SCHEMA.finditer(content):
                name, env_var, true_schema, false_schema = match.groups()
                self.schemas[name.strip()] = {'type': 'conditional', 'env_var': env_var.strip(), 'true_schema': true_schema.strip(), 'false_schema': false_schema.strip()}


class SchemaResolver:
    """
    Resolves, composes, and correctly rewrites references ($ref) in schemas
    to be absolute and fully qualified based on a filename stem ID.
    """
    def __init__(self, all_schemas, internal_to_output_map, use_ranges):
        self.all_schemas = all_schemas
        self.internal_to_output_map = internal_to_output_map
        self.use_ranges = use_ranges
        self.resolution_cache = {}
        self.definition_map = self._build_definition_map()

    def _build_definition_map(self):
        """PASS 1: Scan all schemas to map every definition to its parent schema."""
        def_map = {}
        for internal_name, data in self.all_schemas.items():
            if data['type'] in ['literal', 'external_text']:
                try:
                    schema_content = json.loads(data['content'])
                    if 'definitions' in schema_content and isinstance(schema_content['definitions'], dict):
                        for def_name in schema_content['definitions']:
                            if def_name in def_map:
                                print(f"  [WARNING] Definition '{def_name}' is duplicated in both '{def_map[def_name]}' and '{internal_name}'. Using the latter.")
                            def_map[def_name] = internal_name
                except json.JSONDecodeError:
                    continue
        print(f"Built a global map of {len(def_map)} unique definitions.")
        return def_map

    def resolve(self, name):
        """Public method to fully resolve a root schema."""
        if name in self.resolution_cache:
            return self.resolution_cache[name]

        final_schema = self._resolve_recursive(name)
        self._traverse_and_fix_refs_recursive(final_schema) # Pass 2
        self.resolution_cache[name] = final_schema
        return final_schema

    def _resolve_recursive(self, name):
        """Recursively composes a schema by resolving its components."""
        if name not in self.all_schemas:
            raise ValueError(f"Schema '{name}' is referenced but not defined.")
        data = self.all_schemas[name]
        schema_type = data['type']

        if schema_type in ['literal', 'external_text']:
            try:
                return json.loads(data['content'])
            except json.JSONDecodeError as e:
                raise ValueError(f"Invalid JSON in schema '{name}': {e}") from e
        elif schema_type == 'compound':
            keyword = data['keyword']
            resolved_components = [self._resolve_recursive(c) for c in data['components']]
            return {keyword: resolved_components}
        elif schema_type == 'conditional':
            schema_to_resolve = data['true_schema'] if self.use_ranges else data['false_schema']
            return self._resolve_recursive(schema_to_resolve)
        raise TypeError(f"Unknown schema type '{schema_type}' for schema '{name}'")

    def _traverse_and_fix_refs_recursive(self, current_node):
        """
        PASS 2: Traverses a schema and rewrites all unqualified '$ref' values
        to be absolute URIs using the filename stem as the ID.
        """
        if isinstance(current_node, dict):
            if "$ref" in current_node and isinstance(current_node["$ref"], str):
                ref_val = current_node["$ref"]
                
                if '#' not in ref_val and ':' not in ref_val:
                    if ref_val in self.definition_map:
                        parent_schema_internal_name = self.definition_map[ref_val]
                        output_filename_stem = self.internal_to_output_map.get(parent_schema_internal_name)
                        
                        if output_filename_stem:
                            # --- MODIFICATION: Create the ref WITHOUT the .json extension ---
                            new_ref = f"{output_filename_stem}#/definitions/{ref_val}"
                            current_node["$ref"] = new_ref
                        else:
                            print(f"  [WARNING] Cannot create external $ref for '{ref_val}'. Its parent schema '{parent_schema_internal_name}' does not have a defined output file.")
                    else:
                        print(f"  [WARNING] Could not resolve $ref for '{ref_val}'. Definition not found in any schema.")
            
            for value in current_node.values():
                self._traverse_and_fix_refs_recursive(value)

        elif isinstance(current_node, list):
            for item in current_node:
                self._traverse_and_fix_refs_recursive(item)


def main():
    parser = argparse.ArgumentParser(
        description="Generate JSON schema files from C++ header files with corrected cross-references.",
        formatter_class=argparse.RawTextHelpFormatter
    )
    parser.add_argument("primary_header", type=Path, help="Path to the primary C++ header file.")
    parser.add_argument("secondary_header", type=Path, help="Path to the secondary C++ header.")
    parser.add_argument("output_dir", type=Path, help="Directory to save the generated JSON files.")
    parser.add_argument("--roots-file", type=Path, required=True, help="Text file listing root schemas to generate and their output names.")
    parser.add_argument("--use-ranges", action='store_true', help="Simulate env var for conditional schemas.")
    args = parser.parse_args()

    for file_path in [args.primary_header, args.secondary_header, args.roots_file]:
        if not file_path.is_file():
            print(f"Error: Input file not found at '{file_path}'")
            return

    args.output_dir.mkdir(parents=True, exist_ok=True)
    print(f"Output directory is '{args.output_dir.resolve()}'")

    schema_parser = SchemaParser()
    schema_parser.parse_file(args.secondary_header.read_text(), 'secondary')
    schema_parser.parse_file(args.primary_header.read_text(), 'primary')
    print(f"Found a total of {len(schema_parser.schemas)} raw schema definitions.")

    internal_to_output_map = {}
    schemas_to_generate = []
    print(f"Using explicit list of root schemas from '{args.roots_file.name}'")
    for line in args.roots_file.read_text().splitlines():
        clean_line = line.strip()
        if not clean_line or clean_line.startswith('#'): continue
        parts = [p.strip() for p in clean_line.split('=', 1)]
        internal_name, output_stem = (parts[0], parts[1]) if len(parts) > 1 else (parts[0], parts[0])
        if internal_name in schema_parser.schemas:
            internal_to_output_map[internal_name] = output_stem
            schemas_to_generate.append(internal_name)
        else:
            print(f"  [WARNING] Schema '{internal_name}' from roots file not found. Skipping.")

    print(f"\nInitializing resolver for {len(schemas_to_generate)} root schemas...")
    resolver = SchemaResolver(schema_parser.schemas, internal_to_output_map, args.use_ranges)
    
    for internal_name in schemas_to_generate:
        output_filename_stem = internal_to_output_map[internal_name]
        try:
            print(f"  -> Resolving '{internal_name}' -> '{output_filename_stem}.json'...")
            final_schema = resolver.resolve(internal_name)
            
            # --- MODIFICATION: Set the $id WITHOUT the .json extension ---
            # This URI is now the schema's canonical name for the validator.
            final_schema["$id"] = output_filename_stem

            output_path = args.output_dir / f"{output_filename_stem}.json"
            with open(output_path, 'w', encoding='utf-8') as f:
                json.dump(final_schema, f, indent=4)
        except (ValueError, TypeError) as e:
            print(f"    [ERROR] Could not generate schema for '{internal_name}': {e}")

    print("\nScript finished successfully.")


if __name__ == "__main__":
    main()