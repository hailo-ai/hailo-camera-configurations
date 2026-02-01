#include "common.h"

namespace rule_checker {

EvalResult EvalResultBuilder::success() {
    return {true, {}, {}};
}

EvalResult EvalResultBuilder::error(const std::string& rule_id, const std::string& path, 
                                    const std::string& msg, const std::string& rule_location) {
    Violation v;
    v.rule_id = rule_id;
    v.path = path;
    v.message = msg;
    if (!rule_location.empty())
        v.rule_location = rule_location;
    return {false, {v}, {}};
}

EvalResult EvalResultBuilder::info(const std::string& rule_id, const std::string& path, 
                                   const std::string& msg, const std::string& rule_location) {
    Violation v;
    v.rule_id = rule_id;
    v.path = path;
    v.message = msg;
    if (!rule_location.empty())
        v.rule_location = rule_location;
    return {true, {}, {v}};
}

void EvalResultBuilder::merge(EvalResult& target, const EvalResult& other) {
    target.passed = target.passed && other.passed;
    target.error_violations.insert(target.error_violations.end(), 
                                  other.error_violations.begin(), 
                                  other.error_violations.end());
    target.info_violations.insert(target.info_violations.end(), 
                                 other.info_violations.begin(), 
                                 other.info_violations.end());
}

void EvalResultBuilder::promote_errors_to_info(EvalResult& result) {
    result.passed = true;
    result.info_violations.insert(result.info_violations.end(),
                                 result.error_violations.begin(), 
                                 result.error_violations.end());
    result.error_violations.clear();
}

} // namespace rule_checker
