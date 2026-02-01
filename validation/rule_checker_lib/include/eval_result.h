#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <memory>
#include <nlohmann/json.hpp>

namespace rule_checker {

struct Violation {
    std::string rule_id;
    std::string path;
    std::string message;
    std::optional<std::string> rule_location;

    std::string to_string() const;
};

// Evaluation result
struct EvalResult {
    bool passed;
    std::vector<Violation> error_violations;
    std::vector<Violation> info_violations;

    std::vector<Violation> all_violations() const {
        std::vector<Violation> all;
        all.reserve(error_violations.size() + info_violations.size());
        all.insert(all.end(), error_violations.begin(), error_violations.end());
        all.insert(all.end(), info_violations.begin(), info_violations.end());
        return all;
    }

    std::string to_string() const;
};

} // namespace rule_checker
