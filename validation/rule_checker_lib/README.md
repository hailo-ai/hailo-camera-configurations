# Rule Checker Library

This document serves as a comprehensive guide for utilizing and extending the C++ Rule Checker library.

## Building the Library

The project employs `Meson` and `Ninja` as its build system. The library supports two primary build targets:

### Build Target Options

#### 1. Native x86-64 Build (`x86`)

This build target is designed for development and testing on local x86-64 Linux systems. It is ideal for:
- **Local development and debugging**
- **Executing unit tests on the development machine**
- **Utilizing debugging tools such as `gdb` and `valgrind`**
- **Accelerating development cycles without hardware dependencies**

The x86 build leverages the system's native compiler (e.g., `g++` or `clang++`) and links against standard system libraries.

**Cross-Compilation Script Usage:**
```bash
./cross_compile.sh --target x86 --skip-sysroot --skip-deploy
```

**Running C++ Tests:**
```bash
./builddir/unit_tests
export HAILO_CONFIG_BASE_PATH=/local/repo/path/configurations_tree/
./builddir/rules_validation_tests
```

**Executing Python Tests on Production Profiles:**
```bash
cd python
export PYTHONPATH=../builddir:$PYTHONPATH
export HAILO_CONFIG_BASE_PATH=/local/repo/path/configurations_tree/
pytest
```

#### 2. ARM Cross-Compilation Build (`arm-yocto`)

This target cross-compiles the library for ARM-based embedded devices using the Yocto SDK. Use this for:
- **Building for deployment on target ARM devices**
- **Creating production-ready binaries**
- **Integration with Yocto-based embedded Linux systems**
- **Ensuring binary compatibility with the target device's runtime environment**

The arm-yocto build uses the ARM cross-compiler from the Yocto SDK and links against ARM-specific libraries in the SDK's sysroot.

**Key differences from x86-dev:**
- Uses cross-compiler (e.g., `aarch64-poky-linux-gcc`)
- Links against ARM libraries in the Yocto sysroot
- Produces ARM64 binaries that cannot run on x86-64
- Requires Yocto SDK to be installed
- Can optionally deploy directly to target device via SCP

### Cross-Compilation Script Usage

The `cross_compile.sh` script simplifies the cross-compilation process and provides deployment automation.

#### Prerequisites

1. **Install the Yocto SDK** (default location: `/opt/poky/4.0.23`)
2. **Unset LD_LIBRARY_PATH** before running to avoid environment conflicts:
   ```bash
   unset LD_LIBRARY_PATH
   ```

#### Basic Usage Examples

```bash
# Build for H15 target and copy headers and so's to the SDK and target(default)
./cross_compile.sh

# Build and skip deployment to target device
./cross_compile.sh --ip 10.0.0.1 --skip-deploy

# Combined options
./cross_compile.sh --target arm-yocto --clean --ip 10.0.0.50 --sdk-path /custom/path/to/sdk

# Build for x86-64 development
./cross_compile.sh --target x86-dev
```

**arm-yocto workflow:**
```bash
./cross_compile.sh --target arm-yocto --ip 10.0.0.1
# Build output: builddir/librule_checker.so.1.0.0 (ARM binary)
# Automatically copied to:
# - Yocto sysroot for linking other projects
# - Target device for runtime testing
```

## Adding Custom Rules

Rules are the core validation logic in the rule checker. Each rule implements the `IRule` interface and validates a specific aspect of the configuration.

### Overview of the IRule Interface

To define a custom rule, you must implement the `IRule` interface. This interface mandates the following methods:

```cpp
class IRule {
public:
    virtual ~IRule() = default;

    // Returns the identifier for the rule
    virtual std::string id() const = 0;

    // Provides a detailed description of the rule's purpose
    virtual std::string message() const = 0;

    // Specifies the rule's priority (default: CRITICAL). 
    // Rules with SKIP priority will not be executed.
    virtual RulePriority priority() const { return RulePriority::CRITICAL; }

    // Contains the core validation logic for the rule
    virtual EvalResult evaluate(const nlohmann::json& data) const = 0;
};
```

### Understanding EvalResult and Violation

The `EvalResult` structure encapsulates the outcome of rule evaluation, providing a clear distinction between critical errors and informational messages:

```cpp
struct EvalResult {
    bool passed = true;                          // Indicates whether the rule passed
    std::vector<Violation> error_violations;     // Contains blocking errors
    std::vector<Violation> info_violations;      // Contains non-blocking informational messages
};
```

Each `Violation` contains:

```cpp
struct Violation {
    std::string rule_id; // ID of the rule that generated this violation
    std::string path;    // JSON path where the issue occurred (e.g., "/isp/sensor/name")
    std::string message; // Human-readable description of the issue

};
```
### Creating a Custom Rule

Below is an example implementation of a custom rule:

```cpp
class MyCustomRule : public IRule {
public:
    std::string id() const override {
        return "my_custom_rule";
    }

    std::string message() const override {
        return "Custom rule for example validation.";
    }

    EvalResult evaluate(const nlohmann::json& data) const override {
        EvalResult res;

        // Check if "denoise_enabled" exists and retrieve its value
        bool denoise_enabled;
        std::string denoise_enabled_path = "/iq_settings/denoise/enabled";
        res = get_json_value(data, denoise_enabled_path, denoise_enabled, id());
        if (!res.passed) {
            return res; // Fail if the field is missing
        }

        // Check if "hdr_adaptive_AE_enabled" exists and retrieve its value
        bool hdr_adaptive_AE_enabled;
        std::string hdr_adaptive_AE_enabled_path = "/iq_settings/automatic_algorithms/HDRAdaptiveAE/enable";
        res = get_json_value(data, hdr_adaptive_AE_enabled_path, hdr_adaptive_AE_enabled, id());
        if (!res.passed) {
            // Promote errors to informational messages and skip the rule
            EvalResultBuilder::promote_errors_to_info(res);
            return res;
        }

        // Validate the rule logic
        if (denoise_enabled && hdr_adaptive_AE_enabled) {
            return EvalResultBuilder::error(
                id(),
                "Conflict detected in fields: " + denoise_enabled_path + ", " + hdr_adaptive_AE_enabled_path,
                message()
            );
        }

        return EvalResultBuilder::success();
    }
};
```

### Registering Your Rule

To include your custom rule in the default rule set, modify `src/rules.cpp`:

```cpp
void RuleChecker::build_default_rule_engine() {
    // Add your custom rule
    add_rule(std::make_unique<MyCustomRule>());
}
```

### Writing Tests for Your Rule

To ensure your rule works as expected, write unit tests. Below is an example test case:

```cpp
TEST_F(ApplicationInputIstreamsTest, ValidExampleProfile) {
    if (rule.priority() == RulePriority::SKIP) {
        GTEST_SKIP() << "MyCustomRule is set to SKIP priority, skipping tests.";
    }

    // Test with a valid profile
    auto result = rule.evaluate(get_flattend_profile_json());
    EXPECT_TRUE(result.passed) << "Rule should pass with a valid example profile.";
    EXPECT_EQ(result.error_violations.size(), 0) << "There should be no error violations.";

    // Modify the profile to trigger a rule violation
    nlohmann::json modified_profile = get_flattend_profile_json();
    modified_profile["encoded_output_streams"].emplace_back(modified_profile["encoded_output_streams"][0]);
    modified_profile["encoded_output_streams"].emplace_back(modified_profile["encoded_output_streams"][0]);
    modified_profile["encoded_output_streams"].emplace_back(modified_profile["encoded_output_streams"][0]);

    result = rule.evaluate(modified_profile);
    EXPECT_FALSE(result.passed) << "Rule should fail when encoded_output_streams exceeds resolutions.";
    EXPECT_EQ(result.error_violations.size(), 1) << "There should be one error violation.";
}
```

### Running Tests

Run the following commands to execute the tests:

**C++ Tests:**
```bash
export HAILO_CONFIG_BASE_PATH=/local/repo/path/configurations_tree/
./builddir/rules_validation_tests
```

**Python Tests:**
```bash
cd python
export PYTHONPATH=../builddir:$PYTHONPATH
export HAILO_CONFIG_BASE_PATH=/local/repo/path/configurations_tree/
pytest
```
