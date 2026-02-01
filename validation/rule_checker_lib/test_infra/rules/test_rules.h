#pragma once
#include <rules.h>

namespace rule_checker {
namespace test {

class AlwaysPassRule : public IRule {
public:
    std::string id() const override;
    std::string message() const override;
    EvalResult evaluate(const nlohmann::json &data) const override;
};

class AlwaysFailRule : public IRule {
public:
    std::string id() const override;
    std::string message() const override;
    EvalResult evaluate(const nlohmann::json &data) const override;
};

class VertoinExistRule : public IRule {
public:
    std::string id() const override;
    std::string message() const override;
    EvalResult evaluate(const nlohmann::json &data) const override;
};

class RequiredFieldsRule : public IRule {
public:
    std::string id() const override;
    std::string message() const override;
    EvalResult evaluate(const nlohmann::json &data) const override;
};

class VersionFormatRule : public IRule {
public:
    std::string id() const override;
    std::string message() const override;
    EvalResult evaluate(const nlohmann::json &data) const override;
};

class PositiveNumberRule : public IRule {
public:
    std::string id() const override;
    std::string message() const override;
    EvalResult evaluate(const nlohmann::json &data) const override;
};

class ArrayNotEmptyRule : public IRule {
public:
    std::string id() const override;
    std::string message() const override;
    EvalResult evaluate(const nlohmann::json &data) const override;
};

class StringLengthRule : public IRule {
public:
    std::string id() const override;
    std::string message() const override;
    EvalResult evaluate(const nlohmann::json &data) const override;
};

class NestedFieldRule : public IRule {
public:
    std::string id() const override;
    std::string message() const override;
    EvalResult evaluate(const nlohmann::json &data) const override;
};

class EnumValueRule : public IRule {
public:
    std::string id() const override;
    std::string message() const override;
    EvalResult evaluate(const nlohmann::json &data) const override;
};

} // namespace test
} // namespace rule_checker
