#include "rules/denoise_rules.h"
#include "common.h"
#include "nlohmann/json.hpp"

namespace rule_checker {

// --- FastDenoiseSwitchHdrExclusionRule ---

std::string FastDenoiseSwitchHdrExclusionRule::id() const
{
    return "fast_denoise_switch_hdr_exclusion";
}

std::string FastDenoiseSwitchHdrExclusionRule::message() const
{
    return "fast_denoise_switch and HDR cannot be enabled at the same time.";
}

EvalResult FastDenoiseSwitchHdrExclusionRule::evaluate(const nlohmann::json &data) const
{
    const std::string fast_switch_path = "/iq_settings/denoise/fast_denoise_switch";
    const nlohmann::json::json_pointer fast_switch_ptr(fast_switch_path);

    if (!data.contains(fast_switch_ptr) || !data.at(fast_switch_ptr).get<bool>())
    {
        return EvalResultBuilder::success();
    }

    const std::string denoise_enabled_path = "/iq_settings/denoise/enabled";
    const nlohmann::json::json_pointer denoise_enabled_ptr(denoise_enabled_path);

    if (data.contains(denoise_enabled_ptr) && data.at(denoise_enabled_ptr).is_boolean() &&
        data.at(denoise_enabled_ptr).get<bool>())
    {
        return EvalResultBuilder::error(id(), fast_switch_path,
                                        "fast_denoise_switch is enabled but denoise is also enabled (" +
                                            denoise_enabled_path + " is true). "
                                            "denoise must be disabled when fast_denoise_switch is enabled.");
    }

    const std::string hdr_enabled_path = "/iq_settings/hdr/enabled";
    const nlohmann::json::json_pointer hdr_enabled_ptr(hdr_enabled_path);

    if (data.contains(hdr_enabled_ptr) && data.at(hdr_enabled_ptr).is_boolean() &&
        data.at(hdr_enabled_ptr).get<bool>())
    {
        return EvalResultBuilder::error(id(), fast_switch_path,
                                        "fast_denoise_switch is enabled together with HDR (" +
                                            hdr_enabled_path + " is true). " + message());
    }

    return EvalResultBuilder::success();
}

} // namespace rule_checker
