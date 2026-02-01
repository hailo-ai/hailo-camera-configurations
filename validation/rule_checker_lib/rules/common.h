#pragma once

#include "rule_engine.h"
#include "nlohmann/json.hpp"
#include "src/common.h"

namespace rule_checker {

    template <typename T>
    EvalResult get_json_value(const nlohmann::json &data, const std::string &path, T &value, std::string rule_id)
    {
        try {
            const nlohmann::json::json_pointer ptr(path);
            if (data.contains(ptr)) {
                if constexpr (std::is_same_v<T, nlohmann::json>) {
                    value = data.at(ptr);
                } 
                else {
                value = data.at(ptr).get<T>();
                }
                return EvalResultBuilder::success();
            }
            else {
                return EvalResultBuilder::error(rule_id, path, "Key not found in json: " + data.dump());
            }
        } catch (const nlohmann::json::exception&) {
            return EvalResultBuilder::error(rule_id, path, "Failed to parse field.");
        }
    }
}