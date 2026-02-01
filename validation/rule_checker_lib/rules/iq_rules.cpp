#include "rules/iq_rules.h"
#include "nlohmann/json.hpp"
#include "common.h"
#include <vector>

namespace rule_checker {

std::string GammaTableMonotonousRule::id() const
{
    return "gamma_table_monotonous";
}

std::string GammaTableMonotonousRule::message() const
{
    return "Gamma table values must be in monotonically increasing order.";
}

EvalResult GammaTableMonotonousRule::evaluate(const nlohmann::json &data) const
{
    const std::string curve_path = "/iq_settings/automatic_algorithms/AGamma64/curve";
    
    try {
        const nlohmann::json::json_pointer ptr(curve_path);
        if (!data.contains(ptr)) {
            return EvalResultBuilder::info(id(), curve_path, "Gamma curve not found.");
        }

        if (!data.at(ptr).is_array()) {
            return EvalResultBuilder::info(id(), curve_path, "Gamma curve is not an array, skipping check.");
        }
        std::vector<std::vector<int>> curve = data.at(ptr).get<std::vector<std::vector<int>>>();

        for (const auto& sub_curve : curve) {
            for (size_t i = 1; i < sub_curve.size(); ++i) {
            if (sub_curve[i] < sub_curve[i - 1]) {
                return EvalResultBuilder::error(id(), curve_path, message());
            }
            }
        }
    } catch (const nlohmann::json::exception& e) {
        return EvalResultBuilder::error(id(), curve_path, "Failed to parse or access field. Details: " + std::string(e.what()));
    }

    return EvalResultBuilder::success();
}

std::string ExclusiveAeAlgorithmRule::id() const
{
    return "exclusive_ae_algorithm";
}

std::string ExclusiveAeAlgorithmRule::message() const
{
    return "Only one auto exposure algorithm can be enabled.";
}

EvalResult ExclusiveAeAlgorithmRule::evaluate(const nlohmann::json &data) const
{
    std::vector<std::string> ae_paths = {
        "/iq_settings/automatic_algorithms/Aev1/enable",
        "/iq_settings/automatic_algorithms/Aev2/enable",
        "/iq_settings/automatic_algorithms/Aehdr/enable",
        "/iq_settings/automatic_algorithms/AdaptiveAe/enable",
        "/iq_settings/automatic_algorithms/HDRAdaptiveAE/enable"
    };

    std::vector<std::string> enabled_paths;
    for (const auto& path_str : ae_paths) {
        try {
            const nlohmann::json::json_pointer ptr(path_str);
            if (!data.contains(ptr)) {
                return EvalResultBuilder::error(id(), path_str, "AE algorithm enable field not found.");
            }
            if (!data.at(ptr).is_boolean()) {
                return EvalResultBuilder::error(id(), path_str, "AE algorithm enable field is not a boolean.");
            }
            if (data.at(ptr).get<bool>()) {
                enabled_paths.push_back(path_str);
            }
        } catch (const nlohmann::json::exception&) {
            // Path might not exist, which is fine.
        }
    }

    if (enabled_paths.size() > 1) {
        std::string enabled_list = "Enabled AE algorithms: " + 
                                   std::accumulate(std::next(enabled_paths.begin()), enabled_paths.end(), enabled_paths[0],
                                                   [](std::string a, std::string b) { return a + ", " + b; });
        return EvalResultBuilder::error(id(), enabled_list, message());
    }

    return EvalResultBuilder::success();
}

std::string DisableMultipleDenoiseAlgorithmsRule::id() const
{
    return "disable_multiple_denoise_algorithms";
}

std::string DisableMultipleDenoiseAlgorithmsRule::message() const
{
    return "Post ISP denoise is enabled make sure Adpf, 2D and 3D noise reduction algorithms are disabled";
}

EvalResult DisableMultipleDenoiseAlgorithmsRule::evaluate(const nlohmann::json &data) const//TODO #the description doent support the rule sounds like we need all false insted of mutually exclucive
{
    bool denoise_enabled = false;
    bool denoise_bayer = true; // Default to true to not trigger rule if not present

    EvalResult res;
    res = get_json_value(data, "/iq_settings/denoise/enabled", denoise_enabled, id());
    if (!res.passed) {
        return res;
    }
    res = get_json_value(data, "/iq_settings/denoise/bayer", denoise_bayer, id());
    if (!res.passed) {
        EvalResultBuilder::promote_errors_to_info(res);
        return res;
    }
    if (denoise_enabled && !denoise_bayer) {
        std::vector<std::string> denoise_paths = {
            "/iq_settings/automatic_algorithms/ADpf/disable",
            "/iq_settings/automatic_algorithms/A2dnrv5/disable",
            "/iq_settings/automatic_algorithms/A3dnrv1/disable"
        };

        for (const auto& path_str : denoise_paths) {
            bool disabled = true;
            res = get_json_value(data, path_str, disabled, id());
            if (!res.passed)
            {
                return res;
            }
            if (!disabled) {
                return EvalResultBuilder::error(id(), path_str, message());
            }
        }
    } else {
        return EvalResultBuilder::info(id(), "/iq_settings/denoise/enabled", "Denoise is disabled or bayer denoise is enabled, skipping post ISP denoise algorithms check.");
    }

    return EvalResultBuilder::success();
}

std::string DisabledPostProcessingOnDenoiseDisabledRule::id() const
{
    return "disabled_post_processing_on_denoise_disabled";
}

std::string DisabledPostProcessingOnDenoiseDisabledRule::message() const
{
    return "When ai denoise is disabled so should the auto luma feature in the post processing be";
}

EvalResult DisabledPostProcessingOnDenoiseDisabledRule::evaluate(const nlohmann::json &data) const
{
    bool denoise_enabled = true; // Default to true to not trigger rule if not present
    EvalResult res;
    res = get_json_value(data, "/iq_settings/denoise/enabled", denoise_enabled, id());
    if (!res.passed) {
        return res;
    }
    if (!denoise_enabled) {
        bool auto_luma = false;
        res = get_json_value(data, "/iq_settings/automatic_algorithms/ACprocPost/auto_luma", auto_luma, id());
        if (!res.passed) {
            return res;
        }
        if (auto_luma) {
            return EvalResultBuilder::error(id(), "/iq_settings/automatic_algorithms/ACprocPost/auto_luma", message());
        }
    }
    else {
        return EvalResultBuilder::info(id(), "/iq_settings/denoise/enabled", "Denoise is enabled, skipping auto luma check.");
    }

    return EvalResultBuilder::success();
}

std::string ManualContrastAndBrightnessDuringHistogramEqualizationRule::id() const
{
    return "manual_contrast_and_brightness_during_histogram_equalization";
}

std::string ManualContrastAndBrightnessDuringHistogramEqualizationRule::message() const
{
    return "When histogram equalization is enabled and auto luma isn't, manual contrast must be set to 1 and manual brightness must be set to 0";
}

EvalResult ManualContrastAndBrightnessDuringHistogramEqualizationRule::evaluate(const nlohmann::json &data) const
{
    bool denoise_enabled = false;
    EvalResult res;
    res = get_json_value(data, "/iq_settings/denoise/enabled", denoise_enabled, id());
    if (!res.passed) {
        return res;
    }

    bool auto_luma = true;
    res = get_json_value(data, "/iq_settings/automatic_algorithms/ACprocPost/auto_luma", auto_luma, id());
    if (!res.passed) {
        return res;
    }

    bool histogram_equalization = false;
    res = get_json_value(data, "/iq_settings/automatic_algorithms/ACprocPost/histogram_equalization", histogram_equalization, id());
    if (!res.passed) {
        return res;
    }

    if (denoise_enabled && !auto_luma && histogram_equalization) {
        std::vector<int> manual_contrast;
        EvalResult res;
        res = get_json_value(data, "/iq_settings/automatic_algorithms/ACprocPost/manual_contrast", manual_contrast, id());
        if (!res.passed) {
            return res;
        }
        for (int val : manual_contrast) {
            if (val != 1) {
                return EvalResultBuilder::error(id(), "/iq_settings/automatic_algorithms/ACprocPost/manual_contrast", message());//TODO get legit error
            }
        }

        std::vector<int> manual_brightness;
        res = get_json_value(data, "/iq_settings/automatic_algorithms/ACprocPost/manual_brightness", manual_brightness, id());
        if (!res.passed) {
            return res;
        }
        for (int val : manual_brightness) {
            if (val != 0) {
                return EvalResultBuilder::error(id(), "/iq_settings/automatic_algorithms/ACprocPost/manual_brightness", message());
            }
        }
    }
    else {
        return EvalResultBuilder::info(id(), "/iq_settings/automatic_algorithms/ACprocPost", "Conditions for manual contrast and brightness check not met, skipping.");
    }

    return EvalResultBuilder::success();
}

std::string AdaptiveAePriorityRangesContinuousRule::id() const
{
    return "adaptiveae_priority_ranges_continuous";
}

std::string AdaptiveAePriorityRangesContinuousRule::message() const
{
    return "Priority table ranges must be continuous with no gaps.";
}

EvalResult AdaptiveAePriorityRangesContinuousRule::evaluate(const nlohmann::json &data) const
{
    bool enabled = false;
    EvalResult res;
    res = get_json_value(data, "/iq_settings/automatic_algorithms/AdaptiveAe/enable", enabled, id());
    if (!res.passed) {
        return res;
    }
    if (!enabled) {
        return EvalResultBuilder::info(id(), "/iq_settings/automatic_algorithms/AdaptiveAe/enable", "AdaptiveAe is disabled, skipping priority ranges continuity check.");
    }

    const nlohmann::json::json_pointer priority_ptr("/iq_settings/automatic_algorithms/AdaptiveAe/priority");
    if (!data.contains(priority_ptr)){
        return EvalResultBuilder::error(id(), "/iq_settings/automatic_algorithms/AdaptiveAe/priority", "Priority table not found.");
    }

    if (!data.at(priority_ptr).is_array()) {
        return EvalResultBuilder::info(id(), "/iq_settings/automatic_algorithms/AdaptiveAe/priority", "Priority table is not an array, skipping check.");
    }

    auto priority_table = data.at(priority_ptr);
    if (priority_table.size() <= 1) {
        return EvalResultBuilder::success();
    }

    try {
        double tolerance = 0.000001; //TODO # is it correct to leave range_element_index: 0 ? and not go over more tables?
        int allow_overlap = true;
        int range_element_index = 0;
        double last_end = priority_table[0][range_element_index][1].get<double>();
        for (size_t i = 1; i < priority_table.size(); ++i) {
            double current_start = priority_table[i][range_element_index][0].get<double>();
            if (!allow_overlap && current_start < last_end) {
                return EvalResultBuilder::error(id(), "/iq_settings/automatic_algorithms/AdaptiveAe/priority", "Priority table ranges must not overlap.");
            }
            if (std::abs(current_start - last_end) > tolerance) {
                return EvalResultBuilder::error(id(), "/iq_settings/automatic_algorithms/AdaptiveAe/priority", message());
            }
            last_end = priority_table[i][range_element_index][1].get<double>();
        }
    } catch (const nlohmann::json::exception& e) {
        return EvalResultBuilder::error(id(), "/iq_settings/automatic_algorithms/AdaptiveAe/priority", "Failed to parse priority table. Details: " + std::string(e.what()));
    }

    return EvalResultBuilder::success();
}

std::string HdrAdaptiveAePriorityRangesContinuousRule::id() const
{
    return "hdr-adaptiveae_priority_ranges_continuous";
}

std::string HdrAdaptiveAePriorityRangesContinuousRule::message() const
{
    return "Priority table ranges must be continuous with no gaps.";
}

EvalResult HdrAdaptiveAePriorityRangesContinuousRule::evaluate(const nlohmann::json &data) const
{
    bool enabled = false;
    EvalResult res;
    res = get_json_value(data, "/iq_settings/automatic_algorithms/HDRAdaptiveAE/enable", enabled, id());
    if (!res.passed) {
        return res;
    }
    if (!enabled) {
        return EvalResultBuilder::info(id(), "/iq_settings/automatic_algorithms/HDRAdaptiveAE/enable", "HDRAdaptiveAE is disabled, skipping priority ranges continuity check.");
    }

    const nlohmann::json::json_pointer priority_ptr("/iq_settings/automatic_algorithms/HDRAdaptiveAE/priority");
    if (!data.contains(priority_ptr)){
        return EvalResultBuilder::error(id(), "/iq_settings/automatic_algorithms/HDRAdaptiveAE/priority", "Priority table not found.");
    }
    if (!data.at(priority_ptr).is_array()) {
        return EvalResultBuilder::info(id(), "/iq_settings/automatic_algorithms/HDRAdaptiveAE/priority", "Priority table is not an array, skipping check.");
    }

    auto priority_table = data.at(priority_ptr);
    if (priority_table.size() <= 1) {
        return EvalResultBuilder::success();
    }

    try {
        double tolerance = 0.000001;
        int allow_overlap = true;
        int range_element_index = 0;
        double last_end = priority_table[0][range_element_index][1].get<double>();
        for (size_t i = 1; i < priority_table.size(); ++i) {
            double current_start = priority_table[i][range_element_index][0].get<double>();
            if (!allow_overlap && current_start < last_end) {
                return EvalResultBuilder::error(id(), "/iq_settings/automatic_algorithms/HDRAdaptiveAE/priority", "Priority table ranges must not overlap.");
            }
            if (std::abs(current_start - last_end) > tolerance) {
                return EvalResultBuilder::error(id(), "/iq_settings/automatic_algorithms/HDRAdaptiveAE/priority", message());
            }
            last_end = priority_table[i][range_element_index][1].get<double>();
        }
    } catch (const nlohmann::json::exception& e) {
        return EvalResultBuilder::error(id(), "/iq_settings/automatic_algorithms/HDRAdaptiveAE/priority", "Failed to parse priority table. Details: " + std::string(e.what()));
    }

    return EvalResultBuilder::success();
}

std::string DmscRangeCheckRule::id() const
{
    return "dmsc_range_check";
}

std::string DmscRangeCheckRule::message() const
{
    return "For DMSC block, t2 < t3 < t4 < 2047.";
}

EvalResult DmscRangeCheckRule::evaluate(const nlohmann::json &data) const
{
    bool enabled = false;
    EvalResult res = get_json_value(data, "/iq_settings/automatic_algorithms/ADmscv2/enable", enabled, id());
    if (!res.passed) {
        return res;
    }

    if (!enabled) {
        return EvalResultBuilder::info(id(), "/iq_settings/automatic_algorithms/ADmscv2/enable", "ADmscv2 is disabled, skipping DMSC range check.");
    }

    std::vector<int> t1_arr = {}, t2_shift_arr = {}, t3_arr = {}, t4_shift_arr = {};
    res = get_json_value(data, "/iq_settings/automatic_algorithms/ADmscv2/dmsc_sharpen_t1", t1_arr, id());
    if (!res.passed) {
        return res;
    }
    res = get_json_value(data, "/iq_settings/automatic_algorithms/ADmscv2/dmsc_sharpen_t2_shift", t2_shift_arr, id());
    if (!res.passed) {
        return res;
    }
    res = get_json_value(data, "/iq_settings/automatic_algorithms/ADmscv2/dmsc_sharpen_t3", t3_arr, id());
    if (!res.passed) {
        return res;
    }
    res = get_json_value(data, "/iq_settings/automatic_algorithms/ADmscv2/dmsc_sharpen_t4_shift", t4_shift_arr, id());
    if (!res.passed) {
        return res;
    }
    //(automatic_algorithms.ADmscv2.dmsc_sharpen_t1 + (2 << automatic_algorithms.ADmscv2.dmsc_sharpen_t2_shift)) < automatic_algorithms.ADmscv2.dmsc_sharpen_t3
    //(automatic_algorithms.ADmscv2.dmsc_sharpen_t3 + (2 << automatic_algorithms.ADmscv2.dmsc_sharpen_t4_shift)) < 2047
    for (size_t i = 0; i < t1_arr.size(); ++i) {
        int t1 = t1_arr[i];
        int t2_shift = t2_shift_arr[i];
        int t3 = t3_arr[i];
        int t4_shift = t4_shift_arr[i];

        int t2 = t1 + (2 << t2_shift);
        int t4 = t3 + (2 << t4_shift);

        if (t2 >= t3) {
            return EvalResultBuilder::error(id(), "/iq_settings/automatic_algorithms/ADmscv2", message());
        }
        if (t4 >= 2047) {
            return EvalResultBuilder::error(id(), "/iq_settings/automatic_algorithms/ADmscv2", message());
        }
    }

    return EvalResultBuilder::success();
}

std::string AlgorithmsArraySizeMatchRule::id() const
{
    return "algorithms_array_size_match";
}

std::string AlgorithmsArraySizeMatchRule::message() const
{
    return "When an algorithm is enabled, all top-level numeric arrays must have the same length as 'gains'.";
}

EvalResult AlgorithmsArraySizeMatchRule::evaluate(const nlohmann::json &data) const
{
    const std::string algorithms_path = "/iq_settings/automatic_algorithms";
    try {
        const nlohmann::json::json_pointer ptr(algorithms_path);
        if (!data.contains(ptr)) {
            return EvalResultBuilder::error(id(), algorithms_path, "Automatic algorithms not found.");
        }

        const auto& algorithms = data.at(ptr);
        if (!algorithms.is_object()) {
            return EvalResultBuilder::error(id(), algorithms_path, "Automatic algorithms is not an object.");
        }

        for (auto it = algorithms.begin(); it != algorithms.end(); ++it) {
            const std::string algorithm_path = algorithms_path + "/" + it.key();
            bool enabled = false;
            EvalResult res = get_json_value(data, algorithm_path + "/enable", enabled, id());
            if (!res.passed) {
                EvalResultBuilder::promote_errors_to_info(res);
                return res;
            }

            if (enabled) {
                if (!it.value().contains("gains")) {
                    return EvalResultBuilder::info(id(), algorithm_path + "/gains", "'gains' field not found, skipping array size match check.");
                }

                const auto& gains = it.value()["gains"];
                if (!gains.is_array()) {
                    return EvalResultBuilder::error(id(), algorithm_path + "/gains", "'gains' is not an array.");
                }

                size_t gains_size = gains.size();
                for (auto field_it = it.value().begin(); field_it != it.value().end(); ++field_it) {
                    if (field_it.key() == "gains" || !field_it.value().is_array()) {
                        continue;
                    }

                    if (field_it.value().size() != gains_size) {
                        return EvalResultBuilder::error(id(), algorithm_path + "/" + field_it.key(),
                                                 "Array size does not match 'gains' size.");
                    }
                }
            }
        }
    } catch (const nlohmann::json::exception& e) {
        return EvalResultBuilder::error(id(), algorithms_path, "Failed to parse or access field. Details: " + std::string(e.what()));
    }

    return EvalResultBuilder::success();
}
} // namespace rule_checker
