#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include "eval_result.h"

namespace rule_checker {

/**
 * @brief Validate Factory function to create, configure, and run a RuleChecker
 * 
 * @param json_content JSON content as a string
 * @return EvalResult 
 */
EvalResult validate_rules(const std::string& json_content);

/**
 * Factory function to validate a profile config with flattening
 * @param profile_path Path to the profile configuration file to flatten and validate
 * @param base_path Optional base path for resolving absolute paths
 */
EvalResult validate_rules_on_profile(const std::string& profile_path, const std::string& base_path = "");

} // namespace rule_checker