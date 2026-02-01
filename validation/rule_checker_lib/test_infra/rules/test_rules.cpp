#include "test_rules.h"
#include "rule_checker.h"
#include "common.h"
#include <regex>

namespace rule_checker {
    void RuleChecker::build_default_rule_engine()
    {
        rule_engine = RuleEngine();
    }
}
namespace rule_checker {
namespace test {

std::string AlwaysPassRule::id() const {
    return "AlwaysPass";
}

std::string AlwaysPassRule::message() const {
    return "This rule always passes.";
}

EvalResult AlwaysPassRule::evaluate(const nlohmann::json &data) const {
    return EvalResultBuilder::success();
}

std::string AlwaysFailRule::id() const {
    return "AlwaysFail";
}

std::string AlwaysFailRule::message() const {
    return "This rule always fails.";
}

EvalResult AlwaysFailRule::evaluate(const nlohmann::json &data) const {
    return EvalResultBuilder::error(id(), "/", "This rule is designed to fail.");
}

std::string VertoinExistRule::id() const {
    return "VertoinExistRule";
}

std::string VertoinExistRule::message() const {
    return "This rule check if vertion field exists";
}

EvalResult VertoinExistRule::evaluate(const nlohmann::json &data) const {
    if (data.contains("version")) {
        return EvalResultBuilder::success();
    } else {
        return EvalResultBuilder::error(id(), "/", "The 'vertion' field is missing.");
    }
}

std::string RequiredFieldsRule::id() const {
    return "RequiredFieldsRule";
}

std::string RequiredFieldsRule::message() const {
    return "Validates that required fields exist";
}

EvalResult RequiredFieldsRule::evaluate(const nlohmann::json &data) const {
    EvalResult result = EvalResultBuilder::success();
    std::vector<std::string> required_fields = {"name", "type", "id"};
    
    for (const auto& field : required_fields) {
        if (!data.contains(field)) {
            EvalResultBuilder::merge(result,EvalResultBuilder::error(id(), "/" + field, "Required field '" + field + "' is missing."));
        }
    }
    
    return result;
}

std::string VersionFormatRule::id() const {
    return "VersionFormatRule";
}

std::string VersionFormatRule::message() const {
    return "Validates version follows semantic versioning format (X.Y.Z)";
}

EvalResult VersionFormatRule::evaluate(const nlohmann::json &data) const {
    if (!data.contains("version")) {
        return EvalResultBuilder::error(id(), "/version", "Version field is missing.");
    }
    
    if (!data["version"].is_string()) {
        return EvalResultBuilder::error(id(), "/version", "Version must be a string.");
    }
    
    std::string version = data["version"].get<std::string>();
    std::regex version_regex(R"(^\d+\.\d+\.\d+$)");
    
    if (!std::regex_match(version, version_regex)) {
        return EvalResultBuilder::error(id(), "/version", "Version '" + version + "' does not match semantic versioning format (X.Y.Z).");
    }
    
    return EvalResultBuilder::success();
}

std::string PositiveNumberRule::id() const {
    return "PositiveNumberRule";
}

std::string PositiveNumberRule::message() const {
    return "Validates that numeric fields are positive";
}

EvalResult PositiveNumberRule::evaluate(const nlohmann::json &data) const {
    EvalResult result = EvalResultBuilder::success();
    
    if (data.contains("count") && data["count"].is_number()) {
        int count = data["count"].get<int>();
        if (count <= 0) {
            EvalResultBuilder::merge(result,EvalResultBuilder::error(id(), "/count", "Count must be positive, got: " + std::to_string(count)));
        }
    }
    
    if (data.contains("value") && data["value"].is_number()) {
        double value = data["value"].get<double>();
        if (value <= 0) {
            EvalResultBuilder::merge(result,EvalResultBuilder::error(id(), "/value", "Value must be positive, got: " + std::to_string(value)));
        }
    }
    
    return result;
}

std::string ArrayNotEmptyRule::id() const {
    return "ArrayNotEmptyRule";
}

std::string ArrayNotEmptyRule::message() const {
    return "Validates that array fields are not empty";
}

EvalResult ArrayNotEmptyRule::evaluate(const nlohmann::json &data) const {
    if (!data.contains("items")) {
        return EvalResultBuilder::error(id(), "/items", "Items field is missing.");
    }
    
    if (!data["items"].is_array()) {
        return EvalResultBuilder::error(id(), "/items", "Items must be an array.");
    }
    
    if (data["items"].empty()) {
        return EvalResultBuilder::error(id(), "/items", "Items array cannot be empty.");
    }
    
    return EvalResultBuilder::success();
}

std::string StringLengthRule::id() const {
    return "StringLengthRule";
}

std::string StringLengthRule::message() const {
    return "Validates string fields meet length requirements";
}

EvalResult StringLengthRule::evaluate(const nlohmann::json &data) const {
    EvalResult result = EvalResultBuilder::success();
    
    if (data.contains("name")) {
        if (!data["name"].is_string()) {
            EvalResultBuilder::merge(result,EvalResultBuilder::error(id(), "/name", "Name must be a string."));
        } else {
            std::string name = data["name"].get<std::string>();
            if (name.length() < 3) {
                EvalResultBuilder::merge(result,EvalResultBuilder::error(id(), "/name", "Name must be at least 3 characters long."));
            }
            if (name.length() > 50) {
                EvalResultBuilder::merge(result,EvalResultBuilder::error(id(), "/name", "Name must be at most 50 characters long."));
            }
        }
    }
    
    return result;
}

std::string NestedFieldRule::id() const {
    return "NestedFieldRule";
}

std::string NestedFieldRule::message() const {
    return "Validates nested object structure";
}

EvalResult NestedFieldRule::evaluate(const nlohmann::json &data) const {
    if (!data.contains("config")) {
        return EvalResultBuilder::error(id(), "/config", "Config object is missing.");
    }
    
    if (!data["config"].is_object()) {
        return EvalResultBuilder::error(id(), "/config", "Config must be an object.");
    }
    
    const auto& config = data["config"];
    
    if (!config.contains("enabled")) {
        return EvalResultBuilder::error(id(), "/config/enabled", "Config.enabled is missing.");
    }
    
    if (!config["enabled"].is_boolean()) {
        return EvalResultBuilder::error(id(), "/config/enabled", "Config.enabled must be a boolean.");
    }
    
    return EvalResultBuilder::success();
}

std::string EnumValueRule::id() const {
    return "EnumValueRule";
}

std::string EnumValueRule::message() const {
    return "Validates field values are from allowed set";
}

EvalResult EnumValueRule::evaluate(const nlohmann::json &data) const {
    if (!data.contains("status")) {
        return EvalResultBuilder::error(id(), "/status", "Status field is missing.");
    }
    
    if (!data["status"].is_string()) {
        return EvalResultBuilder::error(id(), "/status", "Status must be a string.");
    }
    
    std::string status = data["status"].get<std::string>();
    std::vector<std::string> allowed_values = {"active", "inactive", "pending"};
    
    bool is_valid = false;
    for (const auto& allowed : allowed_values) {
        if (status == allowed) {
            is_valid = true;
            break;
        }
    }
    
    if (!is_valid) {
        return EvalResultBuilder::error(id(), "/status", "Status '" + status + "' is not a valid value. Allowed: active, inactive, pending.");
    }
    
    return EvalResultBuilder::success();
}

} // namespace test
} // namespace rule_checker
