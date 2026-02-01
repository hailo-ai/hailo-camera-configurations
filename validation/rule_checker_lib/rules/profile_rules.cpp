#include "rules/profile_rules.h"
#include "common.h"
#include "nlohmann/json.hpp"

namespace rule_checker {

std::string ApplicationInputIstreams::id() const
{
    return "encoded_output_streams";
}

std::string ApplicationInputIstreams::message() const
{
    return "The encoded output streams array must be <= application input streams resolutions array length";
}

EvalResult ApplicationInputIstreams::evaluate(const nlohmann::json &data) const
{
    const std::string encoded_streams_path = "/encoded_output_streams";
    const std::string resolutions_path = "/application_settings/application_input_streams/resolutions";

    nlohmann::json encoded_output_streams;
    nlohmann::json application_input_streams_resolutions;

    auto result1 = get_json_value(data, encoded_streams_path, encoded_output_streams, id());
    if (!result1.passed) {
        return result1;
    }

    auto result2 = get_json_value(data, resolutions_path, application_input_streams_resolutions, id());
    if (!result2.passed) {
        return result2;
    }

    // Check if both are arrays
    if (!encoded_output_streams.is_array()) {
        return EvalResultBuilder::error(id(), encoded_streams_path, "Expected an array");
    }
    if (!application_input_streams_resolutions.is_array()) {
        return EvalResultBuilder::error(id(), resolutions_path, "Expected an array");
    }

    if (encoded_output_streams.size() > application_input_streams_resolutions.size()) {
        return EvalResultBuilder::error(id(), encoded_streams_path, "Array sizes do not match. first path: " + encoded_streams_path + " second path size: " +  resolutions_path);
    }

    return EvalResultBuilder::success();

}

} // namespace rule_checker