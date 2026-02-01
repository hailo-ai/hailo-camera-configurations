#pragma once

#include "rule_engine.h"
#include "nlohmann/json.hpp"

namespace rule_checker {

class ApplicationInputIstreams : public IRule {
public:
    ApplicationInputIstreams() = default;
    virtual ~ApplicationInputIstreams() = default;
    std::string id() const override;
    std::string message() const override;
    EvalResult evaluate(const nlohmann::json& data) const override;
};

} // namespace rule_checker