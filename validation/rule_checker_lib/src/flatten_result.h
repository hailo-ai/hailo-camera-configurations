#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <eval_result.h>

namespace rule_checker {

    struct FlatteningError {
        std::string file_path;
        std::string message;
    };
    
    struct FlattenResult {
        nlohmann::json data;
        std::vector<FlatteningError> errors;
    
        void merge(const FlattenResult& other) {
            errors.insert(errors.end(), other.errors.begin(), other.errors.end());
        }
    
        bool has_errors() const {
            return !errors.empty();
        }

    EvalResult convert_to_eval_result(){
        EvalResult result;
        result.passed = errors.empty();
        for (const auto& error : errors) {
            Violation v;
            v.rule_id = "FLATTENER_ERROR";
            v.path = error.file_path;
            v.message = error.message;
            result.error_violations.push_back(v);
        }
        return result;
    }
    };
} // namespace rule_checker