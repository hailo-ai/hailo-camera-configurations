#include <gtest/gtest.h>
#include "eval_result.h"
#include "common.h"
#include "profile_rules.h"
#include "common.h"

using namespace rule_checker;

class ApplicationInputIstreamsTest : public ::testing::Test {
protected:
    ApplicationInputIstreams rule;
};

TEST_F(ApplicationInputIstreamsTest, ValidExampleProfile) {
    if (rule.priority() == RulePriority::SKIP) {
        GTEST_SKIP() << "DmscRangeCheckRule is set to SKIP priority, skipping tests.";
    }
    auto result = rule.evaluate(get_flattend_profile_json());
    EXPECT_TRUE(result.passed) << "Rule should pass with valid example profile";
    EXPECT_EQ(result.error_violations.size(), 0) << "There should be no error violations";

    // Copy the example profile and add an extra encoded stream
    nlohmann::json modified_profile = get_flattend_profile_json();
    modified_profile["encoded_output_streams"].emplace_back(modified_profile["encoded_output_streams"][0]);
    modified_profile["encoded_output_streams"].emplace_back(modified_profile["encoded_output_streams"][0]);
    modified_profile["encoded_output_streams"].emplace_back(modified_profile["encoded_output_streams"][0]);
    
    result = rule.evaluate(modified_profile);
    EXPECT_FALSE(result.passed) << "Rule should fail when encoded_output_streams has more entries than resolutions";
    EXPECT_EQ(result.error_violations.size(), 1) << "There should be one error violation";
}


