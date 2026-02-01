#include "config_transformer.h"
#include "logger.h"

namespace rule_checker {

json ConfigTransformer::process_key_with_content(const json& data, const std::string& key, std::set<std::string>& processed_keys) {
    // Check if there's a corresponding _content key
    std::string content_key = key + CONTENT_SUFFIX;
    if (data.contains(content_key)) {
        // Replace the value with the content
        processed_keys.insert(key);
        processed_keys.insert(content_key);
        LOG_DEBUG("Transformed key '" + key + "' with content from '" + content_key + "'");
        return transform_recursive(data[content_key]);
    } else {
        // No _content key, recursively process the value
        processed_keys.insert(key);
        return transform_recursive(data[key]);
    }
}

void ConfigTransformer::transform_encoded_output_streams(json& result, const std::string& key) {
    const std::string encoded_output_streams_key = "encoded_output_streams";
    if (key != encoded_output_streams_key) {
        return;
    }

    if (result.at(encoded_output_streams_key).is_object()) {
        LOG_DEBUG("Transforming '" + encoded_output_streams_key + "' object to array");
        json streams_array = json::array();
        for (auto stream_it = result.at(encoded_output_streams_key).begin(); stream_it != result.at(encoded_output_streams_key).end(); ++stream_it) {
            streams_array.push_back(stream_it.value());
        }
        result[encoded_output_streams_key] = streams_array;
        LOG_DEBUG(encoded_output_streams_key + " transformed to array with " + std::to_string(streams_array.size()) + " elements");
    }
    else if (!result.at(encoded_output_streams_key).is_array()) {
        LOG_ERROR(encoded_output_streams_key + " is not an object as expected");
    }
}

FlattenResult ConfigTransformer::transform_medialib_json(const json& input_profile_json) {
    LOG_INFO("Transforming medialib JSON to flattened format");
    
    FlattenResult result;
    result.data = transform_recursive(input_profile_json);
    
    LOG_TRACE("Transformed config: " + result.data.dump());
    return result;
}

json ConfigTransformer::transform_recursive(const json& data) {
    // Handle objects - look for key/key_content pairs
    if (data.is_object()) {
        json result = json::object();
        std::set<std::string> processed_keys;
        
        for (auto it = data.begin(); it != data.end(); ++it) {
            const std::string& key = it.key();
            
            // Skip if already processed
            if (processed_keys.count(key) > 0) {
                continue;
            }
            
            // Check if this is a _content key
            bool is_key_with_content_suffix = key.length() > CONTENT_SUFFIX_LENGTH && key.substr(key.length() - CONTENT_SUFFIX_LENGTH) == CONTENT_SUFFIX;
            if (is_key_with_content_suffix) {
                // Skip _content keys, they will be handled by their base key
                continue;
            }
            
            // Check if there's a corresponding _content key
            // if so the key with _content siffix will be deleted and its value will be copied to the base key
            result[key] = process_key_with_content(data, key, processed_keys);

            // check if encoded output streams recived as object with sink keys
            // if so it will tuen it to array without sink keys
            transform_encoded_output_streams(result, key);
        }
        
        return result;
    }
    
    // Handle arrays - recursively process each element
    if (data.is_array()) {
        json result = json::array();
        for (const auto& item : data) {
            result.push_back(transform_recursive(item));
        }
        return result;
    }
    
    // Return primitive values as-is
    return data;
}

} // namespace rule_checker
