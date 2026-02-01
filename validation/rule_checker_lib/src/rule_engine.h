#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <memory>
#include <nlohmann/json.hpp>
#include <eval_result.h>
#include <rules/rules.h>


namespace rule_checker {

class RuleEngine {
    private:
        std::string version;
        std::vector<std::unique_ptr<IRule>> m_rules;
    public:
        std::size_t get_rules_size() const { return m_rules.size(); }
        void add_rule(std::unique_ptr<IRule> rule);
        EvalResult run_all(const nlohmann::json& data) const;
        void clear_rules();
};

}

