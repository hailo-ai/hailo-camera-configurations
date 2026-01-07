---

# Tools

This directory contains helper scripts for configuration management, validation, and code generation within the imaging pipeline.

## Overview

The tools in this directory provide automation for:
- **Schema Generation**: Automatically generate JSON schemas from source code and documentation
- **Configuration Validation**: Validate configuration files against generated schemas
- **Development Workflow**: Streamline the development and maintenance of configuration systems

## Scripts

---

### C++ Header to JSON Schema Generator (`schema_generator.py`)

This Python script automates the extraction and generation of JSON schema files directly from C++ header files. It is designed to treat your C++ source code as the "single source of truth" for your schemas, ensuring that your JSON validation files are always perfectly in sync with your C++ definitions.

The script intelligently parses `nlohmann::json` and `std::string_view` variables, resolves complex compositions (like `allOf`, `oneOf`, `anyOf`), and handles conditional logic based on simulated environment variables.

#### Key Features

*   **Parses Multiple C++ Formats:** Correctly extracts schemas defined as `nlohmann::json` literals, `std::string_view` raw strings, and compound objects.
*   **Resolves Dependencies:** Intelligently handles schemas composed of other schemas (e.g., using `allOf`, `oneOf`, `anyOf`) and can resolve definitions split across multiple files.
*   **Explicit Control via "Roots File":** Instead of guessing, you provide a simple text file that explicitly lists which schemas are the final, "top-level" outputs, example file provided in schemas_name_translation.txt.
*   **Flexible File Renaming:** The roots file allows you to specify a user-friendly filename for each generated JSON, decoupling the output name from the C++ variable name.
*   **Conditional Logic Simulation:** Supports C++ constructs that choose a schema based on a flag, which can be controlled via a command-line argument (`--use-ranges`).

#### Usage

```bash
python schema_generator.py <primary_header> <secondary_header> <output_dir> --roots-file <roots_config_file> [options]
```

#### Parameters

| Parameter | Required | Description |
|-----------|----------|-------------|
| `primary_header`     | Yes | The path to the main C++ header file containing the primary `nlohmann::json` schema definitions.         |
| `secondary_header`   | Yes | The path to a secondary header file that may contain raw schema text (e.g., as `std::string_view`).      |
| `output_dir`         | Yes | The directory where the final `.json` schema files will be saved. It will be created if it doesn't exist. |
| `--roots-file`       | Yes | Path to a text file that lists which schemas to generate and how to name them.            |
| `--use-ranges`       | No | A flag that simulates a C++ environment variable check being true, for conditional schemas. |

---

### Universal Configuration Validator (`config_validator.py`)

A powerful and flexible script for validating a directory tree of JSON configuration files against a set of JSON schemas. It uses a configurable mapping file to associate configuration files with their corresponding schemas, supporting both exact and wildcard filename matching. Its primary feature is a hierarchical error report designed to make sense of thousands of validation errors quickly.

#### Purpose
- Validate entire configuration trees against multiple schemas.
- Provide clear, categorized, and summarized error reports to quickly identify critical issues.
- Decouple validation logic from any single tool, allowing it to be used in any workflow.

#### Key Features

*   **Flexible Mapping Rules:** Uses a simple text file to map configuration filenames to schema files.
*   **Wildcard Support:** Allows for pattern-based mapping, such_as applying `profile_schema.json` to all files matching `*_profile.json`.
*   **Hierarchical Error Reporting:** Groups errors by file and then categorizes them by validator type (e.g., `required`, `type`, `pattern`), making large numbers of errors easy to diagnose.
*   **Comprehensive Summaries:** Generates a summary of error types for each failed file and a final "grand total" summary for the entire validation run.
*   **Recursive Traversal:** Automatically finds and validates all `.json` files within the specified configuration directory.

#### Usage
```bash
python config_validator.py <schema_dir> <config_dir> <mapping_file>
```

#### Parameters

| Parameter | Required | Description |
|-----------|----------|-------------|
| `schema_dir`     | Yes | Path to the directory containing all `.json` schema files. |
| `config_dir`     | Yes | Path to the root directory of configurations to validate. The script will recursively check all `.json` files inside. |
| `mapping_file`   | Yes | Path to a text file that defines the mapping rules between configuration files and schemas. |

---

#### The `mapping_file` Configuration

This file tells the validator which schema to use for which configuration file.

**Format Rules:**

*   Each line represents one mapping rule: `config_filename_or_pattern: schema_filename.json`
*   Lines starting with `#` and blank lines are ignored.
*   The validator prioritizes exact filename matches before checking for wildcard matches.

**Example `mapping_rules.txt`:**

```ini
# This file maps configuration files to their corresponding schemas.

# Use a specific schema for the main component configuration.
main_component.json: component_schema.json

# Use a generic profile schema for all files ending with _profile.json
*_profile.json: profile_schema.json

# Another exact match for a specific configuration
special_config.json: special_schema.json
```

---

### aaa_schema_generation

Generates valid 3aconfig file schemas from imaging source code and documentation. This tool automates the process of creating JSON schemas and can validate existing configuration files against the generated schemas.

#### Purpose
- Extract schema definitions from imaging source code
- Generate JSON Schema files for configuration validation
- Validate existing configuration files against schemas
- Identify unexpected parameters and out-of-range values

#### Usage
```bash
python aaa_schema_generation.py --root <source_path> --out <output_path> [OPTIONS]
```

#### Parameters

| Parameter | Required | Description |
|-----------|----------|-------------|
| `--root` | Yes | Location of the imaging source code directory |
| `--out` | Yes | Location where generated schema files will be saved |
| `--doc` | No | Location of the imaging documentation file (`json_for_parameters.json`) |
| `--validate` | No | Location of folder containing configuration files to validate against generated schemas |

---

### config_manager_excel_generation

Generates Excel formatted sheets for communicating configuration specifications with graphical designers developing the configuration management tool. The output Excel file contains all configuration fields formatted in a designer-friendly layout.

#### Purpose
- Convert JSON profile configurations to Excel format
- Provide structured documentation for UI/UX designers
- Enable non-technical stakeholders to review configuration parameters
- Facilitate collaboration between development and design teams

#### Usage
```bash
python config_manager_excel_generation.py input_json output_excel```

#### Parameters

| Parameter | Required | Description |
|-----------|----------|-------------|
| `input_json` | Yes | Input profile JSON file containing configuration parameters |
| `output_excel` | Yes | Output Excel file path for the formatted configuration sheet |

---

### convert_profiles

Converts profiles from the legacy format (version ≤1.8.0) to the new split format introduced in version 1.9.0. This utility handles the structural changes required for the updated configuration system.

#### Purpose
- Migrate legacy profile configurations to new format
- Handle recursive conversion of profile directory trees
- Support cross-platform path conversion (Windows to Linux)
- Maintain version compatibility during migration

#### Usage
```bash
python convert_profiles.py --source_root <legacy_path> --linux_root <target_path> [OPTIONS]
```

#### Parameters

| Parameter | Required | Default | Description |
|-----------|----------|---------|-------------|
| `--source_root` | Yes | - | Root directory containing profiles in legacy format (supports local and Windows paths) |
| `--linux_root` | Yes | - | Base Linux root location where converted files will reside on target device |
| `--output_root` | No | `cfg_new` | Local output directory for converted profiles |
| `--version` | No | `1.1.0` | Version string to be added to all elements in the converted schema |

---

### fill_defaults

Populates optional parameters from schemas into configuration files, ensuring complete configuration profiles. This utility is recommended to run before `convert_profiles` and `config_manager_excel_generation` to work with complete configurations rather than partial ones.

#### Purpose
- Fill missing optional parameters in configuration files
- Ensure configuration completeness before processing
- Use schema definitions to determine default values
- Prepare configurations for downstream tools

#### Usage
```bash
python fill_defaults.py --input_dir <input_path> --output_dir <output_path> --header_file <schema_file> --linux_root <target_path>```

#### Parameters

| Parameter | Required | Description |
|-----------|----------|-------------|
| `--input_dir` | Yes | Input directory containing configuration files with missing optional parameters |
| `--output_dir` | Yes | Output directory for updated configuration tree with complete profiles |
| `--header_file` | Yes | Media library header file containing schemas for all configuration elements |
| `--linux_root` | Yes | Base Linux root location where files will reside on target device |

---

### xml_to_json

Converts XML files generated by the calibration tool into JSON format for use in profiles (version 1.9.0+). This helper script handles the format transformation required for integrating calibration data into the new profile system.

#### Purpose
- Convert calibration tool XML output to JSON format
- Filter unwanted XML fields during conversion
- Support appending to existing JSON files
- Integrate calibration data into profile workflow

#### Usage
```bash
python xml_to_json.py <xml_file> <json_file> [OPTIONS]
```

#### Parameters

| Parameter | Required | Description |
|-----------|----------|-------------|
| `xml_file` | Yes | Input XML file from calibration tool |
| `json_file` | Yes | Output JSON file for converted results |
| `--skip` | No | Space-separated list of XML fields to exclude from conversion |
| `--append` | No | Enable appending to existing JSON file instead of overwriting |

---

## Installation

Before using any tools in this directory, install the required dependencies:

```bash
# Navigate to tools directory
cd tools/

# Install required packages
pip install -r requirements.txt
```

---

## Common Usage Patterns

### Schema Generation and Validation Workflow
```bash
# 1. Fill in missing optional parameters (recommended first step if aiming for generating screens for the profile manager tool)
python fill_defaults.py \
    --input_dir ./raw_configs \
    --output_dir ./complete_configs \
    --header_file ./schemas/media_library.h \
    --linux_root /etc/imaging/cfg

# 2. Generate schemas from source code
python aaa_schema_generation.py --root ./src --out ./schemas

# 3. Validate the complete configurations using the universal validator
python config_validator.py ./schemas ./complete_configs ./config/mapping_rules.txt
```

### Profile Migration Workflow (Legacy to v1.9.0+)
```bash
# 1. Fill defaults in legacy profiles (recommended first step if aiming for generating screens for the profile manager tool)
python fill_defaults.py \
    --input_dir ./legacy_profiles \
    --output_dir ./complete_legacy \
    --header_file ./schemas/legacy_header.h \
    --linux_root /etc/imaging/cfg

# 2. Convert to new format
python convert_profiles.py \
    --source_root ./complete_legacy \
    --linux_root /etc/imaging/cfg \
    --output_root ./new_format_profiles \
    --version "1.9.0"

# 3. Generate Excel documentation for designers 
python config_manager_excel_generation.py \
    ./new_format_profiles/sample_profile.json \
    ./docs/config_specification.xlsx
```

### Development and Quality Assurance Workflow
```bash
# 1. Generate current schemas from the source
python aaa_schema_generation.py --root ./src --out ./current_schemas

# 2. Validate a large set of production configurations against the newly generated schemas
#    This provides immediate feedback on the impact of any schema changes.
python config_validator.py ./current_schemas ./production_configs ./config/mapping_rules.txt

# 3. Generate documentation for the design team after validation passes
python config_manager_excel_generation.py \
    ./production_configs/sample_profile.json \
    ./docs/config_specification.xlsx
```

### Integration with CI/CD

These tools can be integrated into your CI/CD pipeline:

```yaml
# Example GitHub Actions workflow
- name: Validate Configurations
  run: |
    # This step assumes schemas have been generated or are checked into the repo
    python tools/config_validator.py \
      ./schemas \
      ./test_configs \
      ./config/mapping_rules.txt
```

---

## Development Guidelines

### Adding New Tools

When adding new scripts to this directory:

1. **Naming Convention**: Use descriptive names with underscores (e.g., `config_validator.py`)
2. **Documentation**: Include docstrings and help text for all parameters
3. **Error Handling**: Implement robust error handling with clear error messages
4. **Logging**: Use appropriate logging levels for debugging and monitoring
5. **Testing**: Include unit tests where applicable

### Script Structure

Each script should follow this general structure:

```python
#!/usr/bin/env python3
"""
Script description and purpose.
"""

import argparse
import logging
import sys

def main():
    parser = argparse.ArgumentParser(description='Script description')
    parser.add_argument('--param', required=True, help='Parameter description')
    args = parser.parse_args()
    
    # Script logic here
    
if __name__ == '__main__':
    main()
```

### Best Practices

- **Parameter Validation**: Validate all input parameters before processing
- **Path Handling**: Use `pathlib` for cross-platform path operations
- **Exit Codes**: Return appropriate exit codes (0 for success, non-zero for errors)
- **Progress Indication**: Provide progress feedback for long-running operations
- **Configuration**: Support configuration files for complex parameter sets

## Common Usage Patterns

### Schema Generation and Validation Workflow
```bash
# 1. Fill in missing optional parameters (recommended first step if aiming for generating screens for the profile manager tool)
python fill_defaults.py \
    --input_dir ./raw_configs \
    --output_dir ./complete_configs \
    --header_file ./media-library/hailo-media-library/media_library/src/config_manager/config_manager_schemas.hpp \
    --linux_root /etc/imaging/cfg

# 2. Generate schemas from source code
python aaa_schema_generation.py --root ./src --out ./schemas

# 3. Validate configurations against schemas
python aaa_schema_generation.py --root ./src --out ./schemas --validate ./complete_configs
```

### Profile Migration Workflow (Legacy to v1.9.0+)
```bash
# 1. Fill defaults in legacy profiles (recommended first step if aiming for generating screens for the profile manager tool)
python fill_defaults.py \
    --input_dir ./legacy_profiles \
    --output_dir ./complete_legacy \
    --header_file ./media-library/hailo-media-library/media_library/src/config_manager/config_manager_schemas.hpp \
    --linux_root /etc/imaging/cfg

# 2. Convert to new format
python convert_profiles.py \
    --source_root ./complete_legacy \
    --linux_root /etc/imaging/cfg \
    --output_root ./new_format_profiles \
    --version "1.9.0"

# 3. Generate Excel documentation for designers
python config_manager_excel_generation.py \
    ./new_format_profiles/sample_profile.json \
    ./docs/config_specification.xlsx
```

### Calibration Data Integration Workflow
```bash
# 1. Convert calibration XML to JSON
python xml_to_json.py \
    calibration_output.xml \
    calibration_data.json \
    --skip debug_info temp_values

# 2. Append additional calibration data
python xml_to_json.py \
    additional_calibration.xml \
    calibration_data.json \
    --append

# 3. Integrate into profile workflow
# (Use the JSON output as input to other tools as needed)
```

### Development and Quality Assurance Workflow
```bash
# 1. Generate current schemas
python aaa_schema_generation.py --root ./src --out ./current_schemas

# 2. Compare with previous version for impact analysis
python schema_comparison.py \
    --old ./previous_release/schemas/config_schema.json \
    --new ./current_schemas/config_schema.json

# 3. Validate existing configurations against new schema
python aaa_schema_generation.py \
    --root ./src \
    --out ./current_schemas \
    --validate ./production_configs

# 4. Generate documentation for design team
python config_manager_excel_generation.py \
    ./production_configs/sample_profile.json \
    ./docs/config_specification.xlsx
```

### Integration with CI/CD

These tools can be integrated into your CI/CD pipeline:

```yaml
# Example GitHub Actions workflow
- name: Generate and validate schemas
  run: |
    python tools/aaa_schema_generation.py \
      --root ./imaging/src \
      --out ./generated_schemas \
      --validate ./test_configs
```

## Troubleshooting

### Common Issues

**Missing dependencies:**
```bash
pip install -r requirements.txt
```

**Permission errors:**
- Ensure read access to source directories
- Ensure write access to output directories

**Path issues:**
- Use absolute paths when in doubt
- Check that all specified directories exist

### Getting Help

For script-specific help:
```bash
python script_name.py --help
```

For issues or feature requests, please open an issue in the repository.

## Contributing

When contributing new tools:
1. Follow the existing code style and structure
2. Add comprehensive documentation
3. Include examples in this README
4. Test thoroughly across different environments
5. Update this README with new tool information