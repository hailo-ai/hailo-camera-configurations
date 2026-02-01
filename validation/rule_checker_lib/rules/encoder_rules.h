#pragma once

#include "rule_engine.h"
#include "nlohmann/json.hpp"

namespace rule_checker {

class MonitorFramesIntraPicRateMultiplicationRule : public IRule {
public:
    std::string id() const override;
    std::string message() const override;
    EvalResult evaluate(const nlohmann::json &data) const override;
};

class GopLengthIntraPicRateMultiplicationRule : public IRule {
    public:
        std::string id() const override;
        std::string message() const override;
        EvalResult evaluate(const nlohmann::json &data) const override;
    };

class RateControlPqHdrConstraintRule : public IRule {
    public:
        std::string id() const override;
        std::string message() const override;
        EvalResult evaluate(const nlohmann::json &data) const override;
    };

class ProfileLevelConstraintRule : public IRule {
    public:
        std::string id() const override;
        std::string message() const override;
        EvalResult evaluate(const nlohmann::json &data) const override;
    };

class PictureRcConstraintRule : public IRule {
    public:
        std::string id() const override;
        std::string message() const override;
        EvalResult evaluate(const nlohmann::json &data) const override;
    };

class TargetBitrateWhenPictureRcRule : public IRule {
    public:
        std::string id() const override;
        std::string message() const override;
        EvalResult evaluate(const nlohmann::json &data) const override;
    };
} // namespace rule_checker