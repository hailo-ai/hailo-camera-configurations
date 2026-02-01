#include "rule_checker_interface.h"
#include "rule_checker.h"

namespace rule_checker {

EvalResult validate_rules(const std::string& json_content) {
    RuleChecker rule_checker;
    rule_checker.load_medialib_profile(nlohmann::json::parse(json_content));
    return rule_checker.check();
}

EvalResult validate_rules_on_profile(const std::string& profile_path, const std::string& base_path) {
    RuleChecker rule_checker;
    EvalResult flatten_result = rule_checker.load_profile(profile_path, base_path);
    if (!flatten_result.passed) {
        return flatten_result;
    }
    return rule_checker.check();
}

} // namespace rule_checker