#include "eval_result.h"
#include <sstream>

namespace rule_checker {

std::string Violation::to_string() const {
    std::stringstream ss;
    ss << "Rule ID: " << rule_id << "\n"
       << "  Path: " << path << "\n"
       << "  Message: " << message;
    if (rule_location) {
        ss << "\n  Rule Location: " << *rule_location;
    }
    return ss.str();
}

std::string EvalResult::to_string() const {
    std::stringstream ss;
    ss << "EvalResult(passed=" << (passed ? "True" : "False") << ", ";
    for (const auto& v : error_violations) {
        ss << "\nError Violation:\n" << v.to_string();
    }
    for (const auto& v : info_violations) {
        ss << "\nInfo Violation:\n" << v.to_string();
    }
    ss << "\n)";
    return ss.str();
}

} // namespace rule_checker
