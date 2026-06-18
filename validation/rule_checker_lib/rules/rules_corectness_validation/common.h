#pragma once

#include "rule_checker.h"
//  load_profile

using namespace rule_checker;

inline nlohmann::json get_flattend_profile_json() {
    const char* env_config_base_path = std::getenv("HAILO_CONFIG_BASE_PATH");
    if (!env_config_base_path) {
        throw std::runtime_error("Environment variable PROFILE_PATH is not set");
    }
    std::string base_path(env_config_base_path);
    std::string relative_profile_path = "cfg/hailo15h/imx678/theia_sl410m/4k/profiles/daylight/webserver_daylight/webserver_daylight_basic_profile.json";
    RuleChecker checker;
    EvalResult result = checker.load_profile(base_path + relative_profile_path, base_path);
    if (!result.passed) {
        throw std::runtime_error("Failed to flatten profile: " + result.to_string());
    }
    return checker.get_data();
}