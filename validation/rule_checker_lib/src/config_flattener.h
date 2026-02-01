#pragma once

#include "eval_result.h"
#include "flatten_result.h"
#include <nlohmann/json.hpp>
#include <string>
#include <set>
#include <filesystem>
#include <optional>

namespace rule_checker {

/**
 * ConfigFlattener replaces JSON file path references with their actual content.
 * Supports both board paths (/etc/imaging/cfg) and local repo paths.
 * Maximum flattening depth is 10 to prevent infinite recursion.
 */
class ConfigFlattener {
public:
    /**
     * Constructor
     * @param profile_dir Directory containing the profile file
     * @param base_path Optional base path for resolving absolute paths (e.g., "/etc/imaging/cfg" or local repo path)
     *                  If empty, only relative paths will be resolved
     */
    explicit ConfigFlattener(const std::filesystem::path& profile_dir, const std::string& base_path = "");
    
    /**
     * Flatten a configuration file by replacing path references with content
     * @param config_path Path to the configuration file to flatten
     * @return FlattenResult containing the flattened JSON object and any errors
     */
    FlattenResult flatten_file(const std::filesystem::path& config_path);
    
    /**
     * Add a key to the exclusion list (keys that should NOT be flattened)
     * @param key The key name to exclude from flattening
     */
    void add_key_to_not_flatten(const std::string& key);
    
    /**
     * Get the number of files processed during the last flatten operation
     */
    size_t get_processed_file_count() const { return processed_files_.size(); }
    
private:
    static constexpr int MAX_DEPTH = 10;  // Maximum flattening depth
    static constexpr const char* CFG_PATH_SEGMENT = "/cfg";
    static constexpr const char* DEFAULT_EXCLUDED_KEY = "eis_config_path";
    
    std::filesystem::path profile_dir_;
    std::string base_path_;  // Base path for absolute path resolution (e.g., "/etc/imaging/cfg")
    std::set<std::string> keys_to_not_flatten_;
    std::set<std::string> processed_files_;  // Track processed files for statistics
    
    /**
     * Recursively flatten a JSON value
     * @param data The JSON data to flatten
     * @param current_dir Current directory for resolving relative paths
     * @param current_key The key associated with this value (for exclusion checking)
     * @param depth Current recursion depth
     * @return FlattenResult containing the flattened JSON value and any errors
     */
    FlattenResult flatten_recursive(const nlohmann::json& data, const std::filesystem::path& current_dir, const std::string& current_key, int depth);
    
    /**
     * Check if a value is a JSON path reference that should be flattened
     * @param value The value to check
     * @param key The key associated with this value
     * @return true if this is a path reference that should be flattened
     */
    bool is_json_path_reference(const nlohmann::json& value, const std::string& key) const;
    
    /**
     * Flatten a single JSON path reference by loading and inlining the file content
     * @param path_str The path string to the JSON file
     * @param current_dir Current directory for resolving relative paths
     * @param depth Current recursion depth
     * @return FlattenResult containing the flattened content from the referenced file and any errors
     */
    FlattenResult flatten_json_reference(const std::string& path_str, const std::filesystem::path& current_dir, int depth);
    
    /**
     * Resolve a path string to an actual file path
     * Handles both absolute paths (starting with /) and relative paths
     * @param path_str The path string (relative or absolute)
     * @param current_dir Current directory for resolving relative paths
     * @return Resolved file path
     */
    std::filesystem::path resolve_path(const std::string& path_str, const std::filesystem::path& current_dir) const;
    
    /**
     * Load a JSON file
     * @param file_path Path to the JSON file
     * @return Parsed JSON object, or nullopt if loading fails
     */
    std::optional<nlohmann::json> load_json_file(const std::filesystem::path& file_path) const;
};

} // namespace rule_checker
