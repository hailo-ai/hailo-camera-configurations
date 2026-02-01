#include "rule_checker.h"
#include "config_flattener.h"
#include "config_transformer.h"
#include "flatten_result.h"
#include "logger.h"
#include "common.h"
#include <fstream>
#include <sstream>

namespace rule_checker {

RuleChecker::RuleChecker() 
    : data_(json::object()) {
    LOG_DEBUG("RuleChecker initialized");
    build_default_rule_engine();
}

void RuleChecker::add_rule(std::unique_ptr<IRule> rule)
{
    rule_engine.add_rule(std::move(rule));
}

void RuleChecker::load_data(const nlohmann::json& json_data) {
    LOG_DEBUG("Loading data from JSON object");
    data_ = json_data;
    LOG_DEBUG("Data loaded successfully");
}

EvalResult RuleChecker::load_profile(const std::string& profile_path, const std::string& base_path) {
    LOG_INFO("Loading and flattening profile: " + profile_path);
    
    std::filesystem::path config_path(profile_path);
    
    // Use the directory where the profile is located
    std::filesystem::path profile_dir = config_path.parent_path();
    
    LOG_DEBUG("Using profile directory: " + profile_dir.string());
    if (!base_path.empty()) {
        LOG_DEBUG("Using base path: " + base_path);
    }
    
    // Create flattener and flatten the config
    ConfigFlattener flattener(profile_dir, base_path);
    FlattenResult flatten_result = flattener.flatten_file(config_path);
    if (!flatten_result.has_errors()) {
        data_ = flatten_result.data;
    }
    EvalResult eval_flatten_result = flatten_result.convert_to_eval_result();
    
    if (!eval_flatten_result.passed) {
        LOG_ERROR("Errors occurred during profile flattening.");
    }
    else{
        LOG_INFO("profile config flattened successfully. Processed " + 
            std::to_string(flattener.get_processed_file_count()) + " files.");
    }
    return eval_flatten_result;

}

EvalResult RuleChecker::load_medialib_profile(const nlohmann::json& input_profile_json) {
    LOG_INFO("Transforming medialib profile JSON");
    
    ConfigTransformer transformer;
    FlattenResult result = transformer.transform_medialib_json(input_profile_json);
    
    if (!result.has_errors()) {
        data_ = result.data;
        LOG_INFO("Medialib profile transformed successfully");
    } else {
        LOG_ERROR("Errors occurred during medialib profile transformation");
    }
    EvalResult eval_flatten_result = result.convert_to_eval_result();
    return eval_flatten_result;
}

EvalResult RuleChecker::check() {
    LOG_INFO("=== Starting rule evaluation ===");
    LOG_DEBUG("Total rules to evaluate: " + std::to_string(rule_engine.get_rules_size()));
    
    EvalResult combined = EvalResultBuilder::success();
    
    combined = rule_engine.run_all(data_);

    LOG_INFO("=== Rule evaluation complete ===");
    LOG_INFO("Total errors: " + std::to_string(combined.error_violations.size()));
    LOG_INFO("Total info messages: " + std::to_string(combined.info_violations.size()));
    
    return combined;
}

void RuleChecker::clear_rules(){
    rule_engine.clear_rules();
}

} // namespace rule_checker
