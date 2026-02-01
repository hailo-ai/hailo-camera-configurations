#pragma once
#include <string>
#include <nlohmann/json.hpp>
#include <eval_result.h>

namespace rule_checker {
    enum class RulePriority {
        CRITICAL,
        SKIP
    };

    class IRule {
        public:
            virtual ~IRule() = default;
        
            virtual std::string id() const = 0;
            virtual std::string message() const = 0;
            virtual RulePriority priority() const {
                return RulePriority::CRITICAL;
            }
            virtual EvalResult evaluate(const nlohmann::json& data) const = 0;
        };
}