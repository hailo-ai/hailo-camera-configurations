#include "config_flattener.h"
#include "logger.h"
#include "common.h"
#include <fstream>

namespace rule_checker {

ConfigFlattener::ConfigFlattener(const std::filesystem::path& profile_dir, const std::string& base_path)
    : profile_dir_(std::filesystem::absolute(profile_dir)), base_path_(base_path) {
    // Initialize with default keys to not flatten
    keys_to_not_flatten_.insert(DEFAULT_EXCLUDED_KEY);
    LOG_DEBUG("ConfigFlattener initialized with profile_dir: " + profile_dir_.string());
    if (!base_path_.empty()) {
        LOG_DEBUG("Base path for absolute path resolution: " + base_path_);
    }
}

void ConfigFlattener::add_key_to_not_flatten(const std::string& key) {
    keys_to_not_flatten_.insert(key);
    LOG_DEBUG("Added key to exclusion list: " + key);
}

FlattenResult ConfigFlattener::flatten_file(const std::filesystem::path& config_path) {
    LOG_INFO("Flattening configuration file: " + config_path.string());
    processed_files_.clear();
    
    auto config_data = load_json_file(config_path);
    if (!config_data) {
        std::string error_msg = "Failed to load configuration file: " + config_path.string();
        LOG_ERROR(error_msg);
        return {{}, {{config_path.string(), error_msg}}};
    }
    
    auto flattened_result = flatten_recursive(*config_data, config_path.parent_path(), "", 0);
    LOG_INFO("Flattening complete. Processed " + std::to_string(processed_files_.size()) + " unique files.");
    LOG_TRACE("flattened unified config: " + flattened_result.data.dump());
    
    return flattened_result;
}

FlattenResult ConfigFlattener::flatten_recursive(const nlohmann::json& data, const std::filesystem::path& current_dir, const std::string& current_key, int depth) {
    // Check maximum depth
    if (depth > MAX_DEPTH) {
        std::string msg = "Maximum flattening depth (" + std::to_string(MAX_DEPTH) + ") exceeded. Stopping recursion.";
        LOG_WARN(msg);
        return {data, {{current_dir.string(), msg}}};
    }
    
    // Handle string values that might be path references
    if (is_json_path_reference(data, current_key)) {
        std::string path_str = data.get<std::string>();
        return flatten_json_reference(path_str, current_dir, depth);
    }
    
    // Handle objects - recursively process each key-value pair
    if (data.is_object()) {
        FlattenResult result;
        result.data = nlohmann::json::object();
        for (auto it = data.begin(); it != data.end(); ++it) {
            auto sub_result = flatten_recursive(it.value(), current_dir, it.key(), depth);
            result.data[it.key()] = sub_result.data;
            result.merge(sub_result);
        }
        return result;
    }
    
    // Handle arrays - recursively process each element
    if (data.is_array()) {
        FlattenResult result;
        result.data = nlohmann::json::array();
        for (const auto& item : data) {
            auto sub_result = flatten_recursive(item, current_dir, "", depth);
            result.data.push_back(sub_result.data);
            result.merge(sub_result);
        }
        return result;
    }
    
    // Return primitive values as-is
    return {data, {}};
}

bool ConfigFlattener::is_json_path_reference(const nlohmann::json& value, const std::string& key) const {
    // Don't flatten if the key is in the exclusion list
    if (!key.empty() && keys_to_not_flatten_.count(key) > 0) {
        return false;
    }
    
    // Check if it's a string ending with .json
    if (!value.is_string()) {
        return false;
    }
    
    std::string str = value.get<std::string>();
    
    // Check if it ends with .json
    if (str.length() < 5 || str.substr(str.length() - 5) != ".json") {
        return false;
    }
    
    // Check if it looks like a path (contains / or \)
    if (str.find('/') != std::string::npos || str.find('\\') != std::string::npos) {
        return true;
    }
    
    // Simple filename with .json extension (relative to current directory)
    return true;
}

FlattenResult ConfigFlattener::flatten_json_reference(const std::string& path_str, const std::filesystem::path& current_dir, int depth) {
    try {
        std::filesystem::path file_path = resolve_path(path_str, current_dir);
        
        if (!std::filesystem::exists(file_path)) {
            std::string msg = "Referenced file not found: " + path_str + " -> " + file_path.string() + ". Keeping as path.";
            LOG_WARN(msg);
            return {path_str, {{file_path.string(), msg}}};
        }
        
        LOG_DEBUG("  Flattening reference: " + path_str + " (depth: " + std::to_string(depth) + ")");
        
        // Track processed files for statistics
        std::string canonical_path = std::filesystem::canonical(file_path).string();
        processed_files_.insert(canonical_path);
        
        // Load and recursively flatten the referenced file
        auto referenced_data = load_json_file(file_path);
        if (!referenced_data) {
            std::string msg = "Failed to load referenced file: " + path_str + ". Keeping as path.";
            LOG_WARN(msg);
            return {path_str, {{file_path.string(), msg}}};
        }
        
        return flatten_recursive(*referenced_data, file_path.parent_path(), "", depth + 1);
        
    } catch (const std::exception& e) {
        std::string msg = "Error flattening " + path_str + ": " + e.what() + ". Keeping as path.";
        LOG_WARN(msg);
        return {path_str, {{path_str, msg}}};
    }
}

std::filesystem::path ConfigFlattener::resolve_path(const std::string& path_str, const std::filesystem::path& current_dir) const {
    // Check if it's an absolute path starting with /
    if (!path_str.empty() && path_str[0] == '/') {
        // If we have a base_path, replace the prefix
        if (!base_path_.empty()) {
            // Typically paths look like "/etc/imaging/cfg/..." and we want to replace with base_path + "/cfg/..."
            // Find where "/cfg" starts in the path
            size_t cfg_pos = path_str.find(CFG_PATH_SEGMENT);
            if (cfg_pos != std::string::npos) {
                // Replace everything before /cfg with base_path
                std::string relative_from_cfg = path_str.substr(cfg_pos);
                std::filesystem::path resolved = std::filesystem::path(base_path_) / relative_from_cfg.substr(1); // Remove leading /
                LOG_TRACE("Resolved absolute path: " + path_str + " -> " + resolved.string());
                return resolved;
            } else {
                // Absolute path without /cfg, use as-is
                LOG_WARN("Absolute path does not contain /cfg: " + path_str);
                return std::filesystem::path(path_str);
            }
        } else {
            // No base_path provided, use the absolute path as-is (for board)
            return std::filesystem::path(path_str);
        }
    }
    
    // Relative path - resolve relative to current directory
    return current_dir / path_str;
}

std::optional<nlohmann::json> ConfigFlattener::load_json_file(const std::filesystem::path& file_path) const {
    try {
        std::ifstream file(file_path);
        if (!file.is_open()) {
            LOG_ERROR("Failed to open file: " + file_path.string());
            return std::nullopt;
        }
        
        nlohmann::json data;
        file >> data;
        return data;
        
    } catch (const nlohmann::json::parse_error& e) {
        LOG_ERROR("JSON parse error in " + file_path.string() + ": " + e.what());
        return std::nullopt;
    } catch (const std::exception& e) {
        LOG_ERROR("Error loading file " + file_path.string() + ": " + e.what());
        return std::nullopt;
    }
}

} // namespace rule_checker
