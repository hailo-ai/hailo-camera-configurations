#pragma once

#include "rule_engine.h"
#include "nlohmann/json.hpp"

namespace rule_checker {

class GammaTableMonotonousRule : public IRule {
public:
    std::string id() const override;
    std::string message() const override;
    EvalResult evaluate(const nlohmann::json &data) const override;
};

class ExclusiveAeAlgorithmRule : public IRule {
public:
    std::string id() const override;
    std::string message() const override;
    EvalResult evaluate(const nlohmann::json &data) const override;
};

class DisableMultipleDenoiseAlgorithmsRule : public IRule {
public:
    std::string id() const override;
    std::string message() const override;
    EvalResult evaluate(const nlohmann::json &data) const override;
};

class DisabledPostProcessingOnDenoiseDisabledRule : public IRule {
public:
    std::string id() const override;
    std::string message() const override;
    RulePriority priority() const { return RulePriority::SKIP; }
    EvalResult evaluate(const nlohmann::json &data) const override;
};

class ManualContrastAndBrightnessDuringHistogramEqualizationRule : public IRule {
public:
    std::string id() const override;
    std::string message() const override;
    RulePriority priority() const { return RulePriority::SKIP; }
    EvalResult evaluate(const nlohmann::json &data) const override;
};

class AdaptiveAePriorityRangesContinuousRule : public IRule {
public:
    std::string id() const override;
    std::string message() const override;
    EvalResult evaluate(const nlohmann::json &data) const override;
};

class HdrAdaptiveAePriorityRangesContinuousRule : public IRule {
public:
    std::string id() const override;
    std::string message() const override;
    EvalResult evaluate(const nlohmann::json &data) const override;
};

class DmscRangeCheckRule : public IRule {
public:
    std::string id() const override;
    std::string message() const override;
    RulePriority priority() const { return RulePriority::SKIP; }
    EvalResult evaluate(const nlohmann::json &data) const override;
};

class AlgorithmsArraySizeMatchRule : public IRule {
    public:
        std::string id() const override;
        std::string message() const override;
        EvalResult evaluate(const nlohmann::json &data) const override;
    };
} // namespace rule_checker
