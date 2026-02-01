#include "rule_engine.h"
#include "common.h"

namespace rule_checker {

void RuleEngine::add_rule(std::unique_ptr<IRule> rule) {
    m_rules.emplace_back(std::move(rule));
}

void RuleEngine::clear_rules() {
    m_rules.clear();
}
EvalResult RuleEngine::run_all(const nlohmann::json& data) const {
    EvalResult result;

    for (const auto& rule : m_rules) {
        if (rule->priority() == RulePriority::SKIP) {
            EvalResultBuilder::merge(result, EvalResultBuilder::info(rule->id(), "", "Rule skipped as per its priority setting."));
            continue;
        }
        auto res = rule->evaluate(data);
        EvalResultBuilder::merge(result,res);
    }
    return result;
}

};
