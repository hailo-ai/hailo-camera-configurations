#pragma once

#include "eval_result.h"
#include "flatten_result.h"
#include <nlohmann/json.hpp>
#include <string>
#include <set>

/**
 * @brief gets a gson blob from medialib and transforms it to the expected blob structure
 * 
 */
namespace rule_checker {

    using json = nlohmann::json;
    class ConfigTransformer {
        public:
            /**
             * @brief Transforms the given profile json blob to the expected structure
             * 
             * @param  input_profile_json The input medialib profile json blob
             * @return FlattenResult The transformed profile json blob
             */
            FlattenResult transform_medialib_json(const json& input_profile_json);
        
        private:
            static constexpr const char* CONTENT_SUFFIX = "_content";
            static constexpr size_t CONTENT_SUFFIX_LENGTH = 8;
            
            /**
             * @brief Recursively transforms JSON by replacing path values with their _content counterparts
             * 
             * @param data The JSON data to transform
             * @return json The transformed JSON data
             */
            json transform_recursive(const json& data);
            void transform_encoded_output_streams(json& result, const std::string& key);
            json process_key_with_content(const json& data, const std::string& key, std::set<std::string>& processed_keys);

    };
} // namespace rule_checker