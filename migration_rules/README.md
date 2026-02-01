# Migration Rules DSL guide

This document describes the DSL (Domain-Specific Language) used to define migration rules for configuration files. It is intended for internal developers contributing to schema evolution and migration logic.

## Overview
Each schema update must be accompanied by a migration rule file written in YAML. This file defines how to transform configuration objects from one version to another.
The naming convention for the file needs to be similar to v1.0.0_to_v2.0.0.yaml where the numbers in the source and destination need to be consistent with the semantic version change done in the schema.

## File Structure
A migration rule file contains the following top-level keys:

| Key				| Required	| Type		| Description | 
|-------------------|-----------|-----------|-------------|
| source_version	| Yes		| String	| The semantic version (e.g., "1.0.0") that this migration applies to. | 
| target_version	| Yes		| String	| The semantic version to stamp after successful migration. | 
| description		| No		| String	| A human-readable summary of the changes. | 
| rules				| Yes		| List		| An ordered list of transformation rule objects. Executed sequentially. | 

## Supported Operations

The rules section in the migration yaml file supports the following operations:

### set_required_default
Adds a new required field at the specified path with a default value.

#### Parameters:
*   **path:** JSONPath to the parent object.
*   **field_name:** Name of the new field.
*   **default_value:** Default value to assign.

#### Examples:
```yaml
- operation: 'set_required_default'
  # Path to the parent object that contains the field
  path: '$.modules[*].configuration' 
  field_name: 'log_level'
  # Default value to insert IF 'log_level' is missing
  default_value: 'INFO'
```

### add_field
Adds a new optional field at the specified path.

#### Parameters:
*   **path:** JSONPath to the parent object.
*   **field_name:** Name of the new field.
*   **value:** Value to assign (can be scalar or complex).
*   **internal_only:** optional field denoting whether this field is internal and doesn't need to be exposed in reference profiles

#### Examples:
```yaml
- operation: add_field
  path: "$.power_config"
  field_name: "enabled"
  value: true

- operation: add_field
  path: "$"
  field_name: "hdr_settings"
  value:
    enabled: false
    mode: "auto"
    strength: 50
```

### remove_field
Removes a field from the object.

#### Parameters:
*   **path:** JSONPath to the field.
#### Example:
```yaml
- operation: remove_field
  path: "$.legacy.old_setting"
```

### rename_field
Renames a field while preserving its value.

#### Parameters:
*   **path:** JSONPath to the parent object.
*   **old_name:** Current field name.
*   **new_name:** New field name.

#### Example:
```yaml
- operation: rename_field
  path: "$.settings"
  old_name: "power"
  new_name: "power_config"
```

### move_field
Moves a field to a new location.

#### Parameters:
*   **source_path:** JSONPath to the field.
*   **target_path:** JSONPath to the new parent.
*   **target_name:** Optional new name.

#### Example:
```yaml
- operation: move_field
  source_path: "$.power.level"
  target_path: "$.power.levels"
  target_name: "initial"
```

vset_value
Sets a static value for a field.

#### Parameters:
*   **path:** JSONPath to the field.
*   **value:** New value.
#### Example:
```yaml
- operation: set_value
  path: "$.exposure.mode"
  value: "auto"
```

### transform_value
Applies an expression to modify a field's value.

#### Parameters:
*   **path:** JSONPath to the field.
*   **expression:** Python-style expression using value.
#### Example:
```yaml
- operation: transform_value
  path: "$.exposure.gain"
  expression: "value / 10.0"
```

### to_array
Wraps a scalar or object in a single-element array.
#### Parameters:
*   **path:** JSONPath to the field.
#### Example:
```yaml
- operation: to_array
  path: "$.device_id"
```

### from_array
Extracts an element from an array.

#### Parameters:
*   **path:** JSONPath to the array.
*   **index:** Optional index (default: 0).
#### Example:
```yaml
- operation: from_array
  path: "$.device_ids"
  index: 0
```
### map_enum_values
Maps specific values of a field to new values. Supports both direct renaming and complex transformations with side effects (nested actions). Values not listed in the mapping remain unchanged.

#### Parameters:
*   **path:** JSONPath to the array.
*   **mapping:** A dictionary where keys are the old values. The value for each key can be:
	* **Primitive (String/Int/Bool)**: The new value to replace the old one.
	* **Object**: A dictionary containing:
		* value (Optional): The new value to replace the old one.
		* actions (Optional): A list of other operations (e.g., set_value, add_field) to execute only if this enum value is found.
#### Example:
```yaml
- operation: map_enum_values
  path: "$.sensor_config.mode"
  mapping:
    # Simple Rename: "LEGACY" becomes "STANDARD"
    "LEGACY": "STANDARD"

    # Complex Logic: "OLD_HDR" becomes "HDR_PRO" AND triggers other updates
    "OLD_HDR":
      value: "HDR_PRO"
      actions:
        - operation: set_value
          path: "$.iq_settings.hdr_strength"
          value: 100
        - operation: add_field
          path: "$.metadata"
          field_name: "migrated_from_legacy"
          value: true
```
### set_dynamic_array
Adds a dynamic sized array

#### Parameters:
*   **path:** JSONPath to the array.
*   **rows_from_field:** Which field defines the number of rows (assuming a list of comma separated values).
*   **cols_from_field:** Which field defines the number of columns
*   **value:** Value to assign 

#### Example:
```yaml
- operation: set_dynamic_array
  # Target field to be updated
  path: "$.automatic_algorithms.Awbv2.ccmSaturations"
  
  # Field whose content defines the ROWS count (split by comma)
  rows_from_field: "$.automatic_algorithms.Awbv2.illuorder"
  
  # Field whose length defines the COLUMNS count
  cols_from_field: "$.automatic_algorithms.Awbv2.gains"
  
  # Default value for each cell (optional, defaults to 1.0)
  value: 1.0
```
### clip_value
Clips a value to be in range

#### Parameters:
*   **path:** JSONPath to the member.
*   **min:** Minimal value to clip to
*   **max:** Maximal value to clip to

#### Example:
```yaml
- operation: clip_value
  path: "$.tuning.some_scalar_setting"
  min: 12
  max: 15
```
### clip_array_values
Clips all values in an array to be within a range

#### Parameters:
*   **path:** JSONPath to the array.
*   **min:** Minimal value to clip to
*   **max:** Maximal value to clip to 

#### Example:
```yaml
- operation: clip_array_values
  path: "$.sensor_data.input_gains"
  min: 0.0
  max: 1023.0
```

## Sample Migration Rule File
```yaml
source_version: "1.0.0"
target_version: "2.0.0"
description: "Test for all the different operators"

rules:
- operation: add_field 
  path: "$.automatic_algorithms" 
  field_name: "new_structure" 
  value: 
      enabled: false 
      mode: "auto" 
      strength: 50
      
- operation: remove_field 
  path: "$.automatic_algorithms.Aehdr"
  
- operation: rename_field 
  path: "$.automatic_algorithms.Afv1" 
  old_name: "step" 
  new_name: "step_new"
  
- operation: move_field 
  source_path: "$.automatic_algorithms.Afv1.stableThreshold" 
  target_path: "$.automatic_algorithms.A2dnrv5"
  
- operation: set_value 
  path: "$.automatic_algorithms.Afv1.step_new" 
  value: 3
  
- operation: transform_value 
  path: "$.automatic_algorithms.A3dnrv1.strength" 
  expression: "value / 10.0"
```

For questions or contributions, contact the configuration migration maintainers.
