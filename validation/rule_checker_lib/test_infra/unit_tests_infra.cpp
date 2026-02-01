#include "rule_checker.h"
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include "rules/test_rules.h"

using namespace rule_checker;
using json = nlohmann::json;

// Helper to print violations
void PrintViolations(const EvalResult& result) {
    if (!result.error_violations.empty()) {
        std::cout << "  Error Violations (" << result.error_violations.size() << "):" << std::endl;
        for (const auto &v : result.error_violations) {
            std::cout << "---\n" << v.to_string() << "\n---" << std::endl;
        }
    }
    if (!result.info_violations.empty()) {
        std::cout << "  Info Messages (" << result.info_violations.size() << "):" << std::endl;
        for (const auto &v : result.info_violations) {
            std::cout << "---\n" << v.to_string() << "\n---" << std::endl;
        }
    }
}

TEST(MultiRuleTest, RealWorldScenario_ValidConfig) {
    RuleChecker checker;
    checker.add_rule(std::make_unique<test::VersionFormatRule>());
    checker.add_rule(std::make_unique<test::RequiredFieldsRule>());
    checker.add_rule(std::make_unique<test::PositiveNumberRule>());
    checker.add_rule(std::make_unique<test::StringLengthRule>());
    checker.add_rule(std::make_unique<test::EnumValueRule>());
    checker.add_rule(std::make_unique<test::ArrayNotEmptyRule>());
    checker.add_rule(std::make_unique<test::NestedFieldRule>());

    json data = {
        {"version", "2.1.0"},
        {"name", "ProductionConfig"},
        {"type", "camera_config"},
        {"id", 12345},
        {"count", 5},
        {"value", 120.5},
        {"status", "active"},
        {"items", {"sensor1", "sensor2", "lens1"}},
        {"config", {
            {"enabled", true}
        }}
    };
    checker.load_data(data);
    auto result = checker.check();

    EXPECT_TRUE(result.passed);
    EXPECT_EQ(result.error_violations.size(), 0);
    if (!result.passed) PrintViolations(result);
}

TEST(MultiRuleTest, RealWorldScenario_PartiallyInvalidConfig) {
    RuleChecker checker;
    checker.add_rule(std::make_unique<test::VersionFormatRule>());
    checker.add_rule(std::make_unique<test::RequiredFieldsRule>());
    checker.add_rule(std::make_unique<test::PositiveNumberRule>());
    checker.add_rule(std::make_unique<test::StringLengthRule>());
    checker.add_rule(std::make_unique<test::EnumValueRule>());
    checker.add_rule(std::make_unique<test::ArrayNotEmptyRule>());

    json data = {
        {"version", "2.1"},  // Invalid format
        {"name", "ProductionConfig"},
        {"type", "camera_config"},
        // Missing "id" field
        {"count", 0},  // Invalid: not positive
        {"value", 120.5},
        {"status", "deprecated"},  // Invalid enum value
        {"items", {"sensor1", "sensor2"}}
    };
    checker.load_data(data);
    auto result = checker.check();

    EXPECT_FALSE(result.passed);
    EXPECT_GE(result.error_violations.size(), 4); // version, id, count, status
    if (result.passed) PrintViolations(result);
}


TEST(MultiRuleTest, OneRuleFails_VersionFormat) {
    RuleChecker checker;
    checker.add_rule(std::make_unique<test::VersionFormatRule>());
    checker.add_rule(std::make_unique<test::RequiredFieldsRule>());
    checker.add_rule(std::make_unique<test::PositiveNumberRule>());
    checker.add_rule(std::make_unique<test::StringLengthRule>());

    json data = {
        {"version", "1.0"},  // Invalid version format
        {"name", "ValidName"},
        {"type", "config"},
        {"id", 123},
        {"count", 10}
    };
    checker.load_data(data);
    auto result = checker.check();

    EXPECT_FALSE(result.passed);
    EXPECT_EQ(result.error_violations.size(), 1);
    EXPECT_EQ(result.error_violations[0].rule_id, "VersionFormatRule");
    if (result.passed) PrintViolations(result);
}

TEST(MultiRuleTest, MultipleRulesFail_ThreeViolations) {
    RuleChecker checker;
    checker.add_rule(std::make_unique<test::VersionFormatRule>());
    checker.add_rule(std::make_unique<test::RequiredFieldsRule>());
    checker.add_rule(std::make_unique<test::PositiveNumberRule>());
    checker.add_rule(std::make_unique<test::ArrayNotEmptyRule>());
    checker.add_rule(std::make_unique<test::EnumValueRule>());

    json data = {
        {"version", "invalid"},  // Invalid version
        {"name", "test"},
        {"type", "config"},
        {"id", 123},
        {"count", 0},  // Zero is not positive
        {"status", "unknown"},  // Invalid enum value
        {"items", json::array()}  // Empty array
    };
    checker.load_data(data);
    auto result = checker.check();

    EXPECT_FALSE(result.passed);
    EXPECT_GE(result.error_violations.size(), 4); // Version, PositiveNumber, EnumValue, ArrayNotEmpty
    if (result.passed) PrintViolations(result);
}

TEST(MultiRuleTest, MixedPass_SomeFieldsMissing) {
    RuleChecker checker;
    checker.add_rule(std::make_unique<test::VersionFormatRule>());
    checker.add_rule(std::make_unique<test::PositiveNumberRule>());
    checker.add_rule(std::make_unique<test::StringLengthRule>());
    checker.add_rule(std::make_unique<test::EnumValueRule>());

    json data = {
        {"version", "2.5.1"},
        {"name", "GoodName"},
        {"count", 100}
        // Missing status field - EnumValueRule will fail
        // Missing value field - PositiveNumberRule won't check it
    };
    checker.load_data(data);
    auto result = checker.check();

    EXPECT_FALSE(result.passed);
    EXPECT_EQ(result.error_violations.size(), 1); // Only EnumValueRule fails
    EXPECT_EQ(result.error_violations[0].rule_id, "EnumValueRule");
    if (result.passed) PrintViolations(result);
}

TEST(MultiRuleTest, ComplexNesting_MultipleFail) {
    RuleChecker checker;
    checker.add_rule(std::make_unique<test::NestedFieldRule>());
    checker.add_rule(std::make_unique<test::ArrayNotEmptyRule>());
    checker.add_rule(std::make_unique<test::StringLengthRule>());
    checker.add_rule(std::make_unique<test::PositiveNumberRule>());

    json data = {
        {"config", {{"enabled", "wrong_type"}}},  // Not a boolean
        {"items", json::array()},  // Empty array
        {"name", "ab"},  // Too short
        {"count", -1},  // Negative
        {"value", 0}  // Zero
    };
    checker.load_data(data);
    auto result = checker.check();

    EXPECT_FALSE(result.passed);
    EXPECT_GE(result.error_violations.size(), 5);
    if (result.passed) PrintViolations(result);
}

TEST(MultiRuleTest, EdgeCase_EmptyJSON) {
    RuleChecker checker;
    checker.add_rule(std::make_unique<test::RequiredFieldsRule>());
    checker.add_rule(std::make_unique<test::VersionFormatRule>());
    checker.add_rule(std::make_unique<test::EnumValueRule>());
    checker.add_rule(std::make_unique<test::ArrayNotEmptyRule>());
    checker.add_rule(std::make_unique<test::NestedFieldRule>());

    json data = json::object();  // Empty object
    checker.load_data(data);
    auto result = checker.check();

    EXPECT_FALSE(result.passed);
    EXPECT_GE(result.error_violations.size(), 5); // All rules should fail
    if (result.passed) PrintViolations(result);
}

TEST(MultiRuleTest, BoundaryValues_StringLength) {
    RuleChecker checker;
    checker.add_rule(std::make_unique<test::StringLengthRule>());
    checker.add_rule(std::make_unique<test::VersionFormatRule>());
    checker.add_rule(std::make_unique<test::PositiveNumberRule>());

    // Test minimum valid length
    json data1 = {
        {"name", "abc"},  // Exactly 3 characters (minimum)
        {"version", "0.0.1"},
        {"count", 1}
    };
    checker.load_data(data1);
    auto result1 = checker.check();
    EXPECT_TRUE(result1.passed);

    // Test maximum valid length
    json data2 = {
        {"name", std::string(50, 'a')},  // Exactly 50 characters (maximum)
        {"version", "0.0.1"},
        {"count", 1}
    };
    checker.load_data(data2);
    auto result2 = checker.check();
    EXPECT_TRUE(result2.passed);
}

TEST(TestRuleTest, VertoinExistRule){
  RuleChecker checker;
  checker.add_rule(std::make_unique<test::VertoinExistRule>());
  
  json data_pass = {{"version", "1.0.0"}};
  checker.load_data(data_pass);
  auto result_pass = checker.check();
  EXPECT_TRUE(result_pass.passed);
  if (!result_pass.passed) PrintViolations(result_pass);

  json data_fail = {{"other_key", "value"}};
  checker.load_data(data_fail);
  auto result_fail = checker.check();
  EXPECT_FALSE(result_fail.passed);
  EXPECT_EQ(result_fail.error_violations.size(), 1);
  if (result_fail.passed) PrintViolations(result_fail);
}

TEST(ConfigTransformerTest, SimpleTransformation) {
    RuleChecker checker;
    
    json input = {
        {"config_path", "some/path/to/config.json"},
        {"config_path_content", {
            {"setting1", "value1"},
            {"setting2", 42}
        }}
    };
    
    auto result = checker.load_medialib_profile(input);
    
    EXPECT_TRUE(result.passed);
    const auto& data = checker.get_data();
    
    // The config_path should now contain the content
    EXPECT_TRUE(data.contains("config_path"));
    EXPECT_TRUE(data["config_path"].is_object());
    EXPECT_EQ(data["config_path"]["setting1"], "value1");
    EXPECT_EQ(data["config_path"]["setting2"], 42);
    
    // The config_path_content key should be removed
    EXPECT_FALSE(data.contains("config_path_content"));
}

TEST(ConfigTransformerTest, NestedTransformation) {
    RuleChecker checker;
    
    json input = {
        {"outer", {
            {"inner_path", "nested/config.json"},
            {"inner_path_content", {
                {"nested_value", "test"}
            }},
            {"normal_field", "unchanged"}
        }}
    };
    
    auto result = checker.load_medialib_profile(input);
    
    EXPECT_TRUE(result.passed);
    const auto& data = checker.get_data();
    
    EXPECT_TRUE(data["outer"]["inner_path"].is_object());
    EXPECT_EQ(data["outer"]["inner_path"]["nested_value"], "test");
    EXPECT_FALSE(data["outer"].contains("inner_path_content"));
    EXPECT_EQ(data["outer"]["normal_field"], "unchanged");
}

TEST(ConfigTransformerTest, ArrayTransformation) {
    RuleChecker checker;
    
    json input = {
        {"items", json::array({
            {
                {"item_config", "item1.json"},
                {"item_config_content", {{"id", 1}}}
            },
            {
                {"item_config", "item2.json"},
                {"item_config_content", {{"id", 2}}}
            }
        })}
    };
    
    auto result = checker.load_medialib_profile(input);
    
    EXPECT_TRUE(result.passed);
    const auto& data = checker.get_data();
    
    EXPECT_TRUE(data["items"].is_array());
    EXPECT_EQ(data["items"].size(), 2);
    EXPECT_EQ(data["items"][0]["item_config"]["id"], 1);
    EXPECT_EQ(data["items"][1]["item_config"]["id"], 2);
    EXPECT_FALSE(data["items"][0].contains("item_config_content"));
}

TEST(ConfigTransformerTest, NoContentSuffix) {
    RuleChecker checker;
    
    json input = {
        {"normal_field", "value"},
        {"number", 123},
        {"nested", {
            {"field", "test"}
        }}
    };
    
    auto result = checker.load_medialib_profile(input);
    
    EXPECT_TRUE(result.passed);
    const auto& data = checker.get_data();
    
    // Data should remain unchanged
    EXPECT_EQ(data["normal_field"], "value");
    EXPECT_EQ(data["number"], 123);
    EXPECT_EQ(data["nested"]["field"], "test");
}

TEST(ConfigTransformerTest, MultipleContentKeys) {
    RuleChecker checker;
    
    json input = {
        {"path1", "config1.json"},
        {"path1_content", {{"value", 1}}},
        {"path2", "config2.json"},
        {"path2_content", {{"value", 2}}},
        {"normal", "unchanged"}
    };
    
    auto result = checker.load_medialib_profile(input);
    
    EXPECT_TRUE(result.passed);
    const auto& data = checker.get_data();
    
    EXPECT_EQ(data["path1"]["value"], 1);
    EXPECT_EQ(data["path2"]["value"], 2);
    EXPECT_EQ(data["normal"], "unchanged");
    EXPECT_FALSE(data.contains("path1_content"));
    EXPECT_FALSE(data.contains("path2_content"));
}

TEST(ConfigTransformerTest, EmptyObject) {
    RuleChecker checker;
    
    json input = json::object();
    
    auto result = checker.load_medialib_profile(input);
    
    EXPECT_TRUE(result.passed);
    const auto& data = checker.get_data();
    
    EXPECT_TRUE(data.is_object());
    EXPECT_TRUE(data.empty());
}

TEST(ConfigTransformerTest, ContentWithPrimitives) {
    RuleChecker checker;
    
    json input = {
        {"string_path", "file.json"},
        {"string_path_content", "plain string"},
        {"number_path", "num.json"},
        {"number_path_content", 999},
        {"bool_path", "bool.json"},
        {"bool_path_content", true}
    };
    
    auto result = checker.load_medialib_profile(input);
    
    EXPECT_TRUE(result.passed);
    const auto& data = checker.get_data();
    
    EXPECT_EQ(data["string_path"], "plain string");
    EXPECT_EQ(data["number_path"], 999);
    EXPECT_EQ(data["bool_path"], true);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
