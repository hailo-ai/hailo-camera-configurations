#pragma once

#include "rule_engine.h"
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <memory>

namespace rule_checker {

class RuleChecker {
public:
    RuleChecker();

    void add_rule(std::unique_ptr<IRule> rule);

    void load_data(const nlohmann::json& json_data);
    
    /**
     * Transform a medialib profile JSON blob to the expected structure
     * Replaces key_content values with the original key values, removing _content suffixes
     * @param input_profile_json The input medialib profile JSON blob
     * @return EvalResult containing the transformed JSON and any errors
     */
    EvalResult load_medialib_profile(const nlohmann::json& input_profile_json);
    
    /**
     * Load and flatten a profile configuration file
     * Recursively replaces all JSON path references with their actual content
     * @param profile_path Path to the profile configuration file
     * @param base_path Optional base path for resolving absolute paths (e.g., "/etc/imaging/cfg" on board, 
     *                  or local repo path on PC). If empty, absolute paths will be used as-is.
     * @return An EvalResult containing any errors that occurred during flattening.
     *         If there are errors, the internal data_ member may be incomplete.
     */
    EvalResult load_profile(const std::string& profile_path, const std::string& base_path = "");
    
    /**
     * @brief Check the loaded data against the added rules
     * @return EvalResult 
     */
    EvalResult check();
    
    const nlohmann::json& get_data() const { return data_; }

    void clear_rules();

private:
    RuleEngine rule_engine;
    nlohmann::json data_;

    void build_default_rule_engine();
};

} // namespace rule_checker
