# Validation Rules DSL

A YAML-based Domain Specific Language (DSL) for defining advanced validation rules that complement JSON Schema validation. This DSL enables complex cross-field validation, conditional checks, and business logic constraints that are difficult or impossible to express in standard JSON Schema.

## Overview

The validation rules system works alongside JSON Schema to provide:
- **Cross-field validation**: Compare values across different parts of the configuration
- **Conditional logic**: Apply different validation rules based on configuration state
- **Business rules**: Encode domain-specific constraints and relationships
- **Clear error messages**: Provide meaningful feedback when validation fails

Both the JSON Schema and YAML rules files must be applied together for complete validation of configuration files. The `version` field specifies which component or profile version these rules apply to, ensuring compatibility between schemas, rules, and configuration files.

## File Structure

```yaml
version: ">=1.2.0"  # Component/profile version these rules apply to
rules:              # List of validation rules
  - id: rule_name   # Unique identifier for the rule
    if:             # Condition that must be met for rule to apply
      # condition definition
    checks:         # Validation checks to perform when condition is met
      # check definition
    message: "Error message"  # Human-readable error message
```

## Core Concepts

### Rules
A rule consists of three main components:
- **Condition (`if`)**: Determines when the rule should be evaluated
- **Checks**: The actual validations to perform
- **Message**: Error message displayed when validation fails

### Field References
Fields are referenced using dot notation:
```yaml
field: encoding.hailo_encoder.gop_config.gop_size
field: module1.enable
field: settings.advanced.timeout_ms
```

Array elements can be referenced using bracket notation:
```yaml
field: sensors[0].name
field: tables[*].strength  # All elements in array
```

### Conditions (`if`)
Conditions determine when a rule should be applied. They can be:

#### Simple Field Conditions
```yaml
if:
  field: encoding.input_stream.width
  type: integer
  gte: 1920
```

#### Logical Combinations
```yaml
# All conditions must be true (AND)
if:
  all:
    - field: module1.enable
      eq: true
    - field: module2.enable
      eq: true

# At least one condition must be true (OR)
if:
  any:
    - field: mode
      eq: "auto"
    - field: mode
      eq: "manual"

# Negate a condition (NOT)
if:
  not:
    field: debug_mode
    eq: true
```

### Checks
Checks define the actual validation logic to perform when conditions are met:

#### Simple Checks
```yaml
checks:
  gte: 1000
  lte: 50000
  mod: 1000
```

#### Field-based Checks
```yaml
checks:
  encoding.hailo_encoder.coding_control.rate_control.bitrate.target_bitrate: 40000
```

#### Advanced Check Types
```yaml
checks:
  # Forbid specific combinations
  forbid: true

  # Require multiple conditions
  require:
    - field: backup.enable
      eq: true
    - field: backup.interval
      gte: 300

  # Implication logic (if A then B)
  implies:
    condition:
      field: ssl.enable
      eq: true
    then:
      field: ssl.certificate_path
      exists: true
```

## Operators Reference

### Condition & Check Operators

| Operator | Context | Type(s) | Meaning |
|----------|---------|---------|---------|
| `eq` | if/check | any | Strict equality (must match type and value) |
| `neq` | if/check | any | Strict inequality |
| `eq_numeric` | if/check | number | Numeric equality (e.g. 0 == 0.0 is true) |
| `neq_numeric` | if/check | number | Numeric inequality (e.g. 0 != 1.0 is true) |
| `gt` | if/check | number | Greater than |
| `gte` | if/check | number | Greater than or equal |
| `lt` | if/check | number | Less than |
| `lte` | if/check | number | Less than or equal |
| `in` | if/check | any | Value is in a provided list |
| `not_in` | if/check | any | Value is not in a provided list |
| `regex` | if/check | string | String matches regex pattern |
| `exists` | if/check | boolean | Field presence test |
| `type` | if | type label | Expected type (`integer`, `number`, `string`, `boolean`, `array`, `object`) |

### Numeric Convenience Operators

| Operator | Context | Type(s) | Meaning |
|----------|---------|---------|---------|
| `mod` | check | integer | Divisible by operand (remainder = 0) |
| `is_even` | check | integer | Even integer |
| `is_odd` | check | integer | Odd integer |
| `range` | check | number | Within inclusive range [min, max] |

### String Operators

| Operator | Context | Type(s) | Meaning |
|----------|---------|---------|---------|
| `starts_with` | if/check | string | String starts with specified prefix |
| `ends_with` | if/check | string | String ends with specified suffix |
| `length` | if/check | string | String has exact length |
| `min_length` | if/check | string | String has minimum length |
| `max_length` | if/check | string | String has maximum length |

### Array/Collection Operators

| Operator | Context | Type(s) | Meaning |
|----------|---------|---------|---------|
| `length` | if/check | array | Array has exact number of elements |
| `min_length` | if/check | array | Array has minimum number of elements |
| `max_length` | if/check | array | Array has maximum number of elements |
| `contains` | if/check | array | Array contains specified value |
| `unique` | check | array | All array elements are unique |
| `unique_field` | check | array of objects | Specified field is unique across all objects |
| `for_each: path.*` | if/check | object/array | Iterates over all children of the given object or array path. Inside the loop, field: is relative to the child. |
| `all_numeric_arrays` | check | object | Applies the nested rule to all top-level fields in the current object that are arrays of numbers. Strings, objects, or mixed arrays are ignored. |

### Array Order Operators

| Operator | Context | Type(s) | Meaning |
|----------|---------|---------|---------|
| `monotonic_increasing` | check | array of numbers | Elements are in non-decreasing order (≤) |
| `monotonic_decreasing` | check | array of numbers | Elements are in non-increasing order (≥) |
| `strictly_increasing` | check | array of numbers | Elements are in strictly increasing order (<) |
| `strictly_decreasing` | check | array of numbers | Elements are in strictly decreasing order (>) |

### Array Relationship Operators

| Operator | Context | Type(s) | Meaning |
|----------|---------|---------|---------|
| `arrays_identical` | check | array | Current array is identical to another array field |
| `arrays_same_length` | check | array | Current array has same length as another array field |

### Complex Array Structure Operators

| Operator | Context | Type(s) | Meaning |
|----------|---------|---------|---------|
| `range_continuity` | check | array of arrays | Nested ranges have no gaps between consecutive entries |
| `nested_ranges_valid` | check | array of arrays | Each nested array contains valid [min, max] ranges |

### Array Length Comparison Operators

| Operator                      | Context | Type(s)         | Meaning                                                                                         |
| ----------------------------- | ------- | --------------- | ----------------------------------------------------------------------------------------------- |
| `same_length_as: field`       | check   | array           | The array must have the same total length as the reference array.                               |
| `same_outer_length_as: field` | check   | array of arrays | The **first dimension** length must match the reference array length (nested elements ignored). |
| `same_inner_length_as: field` | check   | array of arrays | Each **inner array length** must match the reference array length.                              |


### Cross-field & Collection Operators

| Operator | Context | Type(s) | Meaning |
|----------|---------|---------|---------|
| `eq_field` | check | any | Equals value from another field |
| `neq_field` | check | any | Not equals value from another field |
| `lt_field` | check | number | Less than value from another field |
| `lte_field` | check | number | Less than or equal to value from another field |
| `gt_field` | check | number | Greater than value from another field |
| `gte_field` | check | number | Greater than or equal to value from another field |
| `mod_field` | check | integer | Current field is divisible by value from another field |
| `sum_fields` | check | number | Current field equals sum of specified fields |
| `product_fields` | check | number | Current field equals product of specified fields |
| `eq_calculated` | check | number | Equals result of mathematical expression |
| `mutually_exclusive` | check | list of fields	| Ensures that at most one field from the list is true/enabled |
| `min_active` | check | list of fields	| Requires at least N of the listed fields to be true/enabled |
| `max_active` | check | list of fields	| Requires no more than N of the listed fields to be true/enabled |
| `exactly_active` | check | list of fields	| Requires exactly N of the listed fields to be true/enabled |

### Specialized Operators

| Operator | Context | Type(s) | Meaning |
|----------|---------|---------|---------|
| `is_valid_ip` | if/check | string | Valid IP address format |
| `ip_in_range` | check | string | IP address within specified CIDR range |
| `ip_gt_field` | check | string | IP address numerically greater than another field |
| `is_valid_email` | if/check | string | Valid email address format |
| `is_valid_url` | if/check | string | Valid URL format |
| `is_valid_uuid` | if/check | string | Valid UUID format |
| `is_valid_json` | if/check | string | Valid JSON string |
| `file_exists` | if/check | string | File path exists on filesystem |
| `dir_exists` | if/check | string | Directory path exists on filesystem |

### Logical & Structural Operators

| Operator | Context | Meaning |
|----------|---------|---------|
| `all` | if | All subconditions must hold (AND logic) |
| `any` | if | At least one subcondition must hold (OR logic) |
| `not` | if | Negates a single subcondition |
| `forbid` | check | If `if` condition holds, this combination is illegal |
| `require` | check | List of mini-conditions that must all hold |
| `implies` | check | If first clause holds, second clause must hold |
| `per_element` | check | Applies nested checks to each element of an array (recursively if arrays of arrays) |

## Examples

### Basic Range Validation
```yaml
- id: valid_bitrate_range
  if:
    field: encoding.hailo_encoder.coding_control.rate_control.bitrate.target_bitrate
    type: integer
  checks:
    gte: 1000
    lte: 50000
    mod: 1000
  message: "Bitrate must be between 1000 and 50000 and divisible by 1000."
```

### Conditional Field Requirements
```yaml
- id: ssl_certificate_required
  if:
    field: server.ssl.enabled
    eq: true
  checks:
    require:
      - field: server.ssl.certificate_path
        exists: true
      - field: server.ssl.private_key_path
        exists: true
  message: "SSL certificate and private key paths are required when SSL is enabled."
```

### Cross-field Validation
```yaml
- id: end_time_after_start
  if:
    all:
      - field: schedule.start_time
        exists: true
      - field: schedule.end_time
        exists: true
  checks:
    field: schedule.end_time
    gt_field: schedule.start_time
  message: "End time must be after start time."
```

### Mutually Exclusive Options
```yaml
- id: mutually_exclusive_modules
  if:
    all:
      - field: module1.enable
        eq: true
      - field: module2.enable
        eq: true
  checks:
    forbid: true
  message: "module1 and module2 cannot both be enabled."
```

### Complex Business Rules
```yaml
- id: high_res_bitrate_constraint
  if:
    all:
      - field: encoding.input_stream.width
        gte: 3840
      - field: encoding.input_stream.height
        gte: 2160
  checks:
    field: encoding.hailo_encoder.coding_control.rate_control.bitrate.target_bitrate
    lte: 40000
  message: "4K video (3840x2160 or higher) must not exceed 40 Mbps bitrate."
```

### Array Order Validation
```yaml
- id: exposure_values_ordered
  if:
    field: camera.exposure_values
    type: array
  checks:
    monotonic_increasing: true
  message: "Exposure values must be in monotonically increasing order."

- id: priority_levels_descending
  if:
    field: task_priorities
    type: array
  checks:
    strictly_decreasing: true
  message: "Task priorities must be in strictly decreasing order."
```

### Array Relationship Validation
```yaml
- id: reference_backup_identical
  if:
    all:
      - field: reference_values
        type: array
      - field: backup_values
        type: array
  checks:
    field: reference_values
    arrays_identical: backup_values
  message: "Reference and backup value arrays must be identical."

- id: input_output_same_size
  if:
    all:
      - field: input_channels
        type: array
      - field: output_channels
        type: array
  checks:
    field: input_channels
    arrays_same_length: output_channels
  message: "Input and output channel arrays must have the same length."
```

### Complex Array Structure Validation
```yaml
- id: priority_ranges_continuous
  if:
    field: priority
    type: array
  checks:
    range_continuity:
      range_element_index: 0    # Index of range array within each priority entry
      allow_overlap: true       # Overlapping ranges are permitted
      sort_first: false         # Don't auto-sort, validate as-is
      tolerance: 0.000001       # Floating-point tolerance for gap detection
  message: "Priority table ranges must be continuous with no gaps."

- id: nested_ranges_format
  if:
    field: calibration_ranges
    type: array
  checks:
    nested_ranges_valid: true
  message: "Each calibration range must be a valid [min, max] array with min <= max."
```

### Optional Field Validation
```yaml
- id: optional_timeout_range
  if:
    field: network.timeout_ms
    exists: true
  checks:
    gte: 1000
    lte: 30000
  message: "When specified, timeout must be between 1000 and 30000 milliseconds."

- id: optional_log_level
  if:
    field: logging.level
    exists: true
  checks:
    in: ["debug", "info", "warn", "error", "fatal"]
  message: "When specified, log level must be one of: debug, info, warn, error, fatal."
```

### String Validation
```yaml
- id: valid_device_name
  if:
    field: device.name
    type: string
  checks:
    starts_with: "DEV_"
    ends_with: "_001"
    min_length: 8
    max_length: 32
  message: "Device name must start with 'DEV_', end with '_001', and be 8-32 characters long."

- id: password_strength
  if:
    field: auth.password
    exists: true
  checks:
    min_length: 8
    regex: "^(?=.*[a-z])(?=.*[A-Z])(?=.*\\d)(?=.*[@$!%*?&])[A-Za-z\\d@$!%*?&]"
  message: "Password must be at least 8 characters with uppercase, lowercase, number, and special character."
```

### Range Validation
```yaml
- id: valid_port_range
  if:
    field: network.port
    type: integer
  checks:
    range: [1024, 65535]
  message: "Port must be between 1024 and 65535."

- id: temperature_sensor_range
  if:
    field: sensors.temperature.value
    type: number
  checks:
    range: [-40.0, 85.0]
  message: "Temperature sensor reading must be between -40.0°C and 85.0°C."
```

### Advanced Cross-field Validation
```yaml
- id: total_memory_validation
  if:
    all:
      - field: memory.heap_size
        exists: true
      - field: memory.stack_size
        exists: true
      - field: memory.total_allocated
        exists: true
  checks:
    field: memory.total_allocated
    sum_fields: [memory.heap_size, memory.stack_size]
  message: "Total allocated memory must equal heap size plus stack size."

- id: area_calculation_check
  if:
    all:
      - field: dimensions.width
        exists: true
      - field: dimensions.height
        exists: true
      - field: dimensions.area
        exists: true
  checks:
    field: dimensions.area
    product_fields: [dimensions.width, dimensions.height]
  message: "Area must equal width times height."

- id: cbr_pq_hdr_constraint
  if:
    all:
      - field: encoding.rate_control
        eq: "CBR"
      - field: encoding.pq_hdr
        exists: true
  checks:
    any:
      - field: encoding.pq_hdr
        eq: -1
      - all:
          - field: encoding.pq_hdr
            gte_field: encoding.qp_min
          - field: encoding.pq_hdr
            lte_field: encoding.qp_max
  message: "When rate control is CBR, pq_hdr must be either -1 or within the range [qp_min, qp_max]."
```