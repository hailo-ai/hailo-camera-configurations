#include <gtest/gtest.h>
#include "eval_result.h"
#include "common.h"
#include "encoder_rules.h"
#include "common.h"

using namespace rule_checker;

// MonitorFramesIntraPicRateMultiplicationRule Tests
class MonitorFramesIntraPicRateMultiplicationRuleTest : public ::testing::Test {
protected:
    MonitorFramesIntraPicRateMultiplicationRule rule;
};

TEST_F(MonitorFramesIntraPicRateMultiplicationRuleTest, MonitorFramesMultiplicationTests) {
    if (rule.priority() == RulePriority::SKIP) {
        GTEST_SKIP() << "MonitorFramesIntraPicRateMultiplicationRule is set to SKIP priority, skipping tests.";
    }
    nlohmann::json test_profile = get_flattend_profile_json();
    
    // Valid example profile
    auto result = rule.evaluate(test_profile);
    EXPECT_TRUE(result.passed) << "Rule should pass with valid example profile. errors: " << result.to_string();
    // Valid multiplication: monitor_frames is a multiple of intra_pic_rate
    test_profile["encoded_output_streams"][0]["encoding"]["encoding"]["hailo_encoder"]["rate_control"]["monitor_frames"] = 60;
    test_profile["encoded_output_streams"][0]["encoding"]["encoding"]["hailo_encoder"]["rate_control"]["intra_pic_rate"] = 30;
    result = rule.evaluate(test_profile);
    EXPECT_TRUE(result.passed) << "Rule should pass when monitor_frames (60) is a multiple of intra_pic_rate (30). errors: " << result.to_string();
    EXPECT_EQ(result.error_violations.size(), 0) << "There should be no error violations";

    // Invalid multiplication: monitor_frames is NOT a multiple of intra_pic_rate
    test_profile["encoded_output_streams"][0]["encoding"]["encoding"]["hailo_encoder"]["rate_control"]["monitor_frames"] = 50;
    test_profile["encoded_output_streams"][0]["encoding"]["encoding"]["hailo_encoder"]["rate_control"]["intra_pic_rate"] = 30;
    result = rule.evaluate(test_profile);
    EXPECT_FALSE(result.passed) << "Rule should fail when monitor_frames (50) is not a multiple of intra_pic_rate (30)";
    EXPECT_EQ(result.error_violations.size(), 1) << "There should be one error violation";

    // Missing monitor_frames field
    test_profile["encoded_output_streams"][0]["encoding"]["encoding"]["hailo_encoder"]["rate_control"].erase("monitor_frames");
    result = rule.evaluate(test_profile);
    EXPECT_TRUE(result.passed) << "Rule should pass (with info) when monitor_frames is missing. errors: " << result.to_string();
    EXPECT_EQ(result.info_violations.size(), 0) << "There should be zero info violation";
}

// GopLengthIntraPicRateMultiplicationRule Tests
class GopLengthIntraPicRateMultiplicationRuleTest : public ::testing::Test {
protected:
    GopLengthIntraPicRateMultiplicationRule rule;
};

TEST_F(GopLengthIntraPicRateMultiplicationRuleTest, GopLengthMultiplicationTests) {
    if (rule.priority() == RulePriority::SKIP) {
        GTEST_SKIP() << "GopLengthIntraPicRateMultiplicationRule is set to SKIP priority, skipping tests.";
    }
    nlohmann::json test_profile = get_flattend_profile_json();
    
    // Valid example profile
    auto result = rule.evaluate(test_profile);
    EXPECT_TRUE(result.passed) << "Rule should pass with valid example profile. errors: " << result.to_string();
    
    // Valid multiplication: gop_length is a multiple of intra_pic_rate
    test_profile["encoded_output_streams"][0]["encoding"]["encoding"]["hailo_encoder"]["rate_control"]["gop_length"] = 90;
    test_profile["encoded_output_streams"][0]["encoding"]["encoding"]["hailo_encoder"]["rate_control"]["intra_pic_rate"] = 30;
    result = rule.evaluate(test_profile);
    EXPECT_TRUE(result.passed) << "Rule should pass when gop_length (90) is a multiple of intra_pic_rate (30). errors: " << result.to_string();
    EXPECT_EQ(result.error_violations.size(), 0) << "There should be no error violations";

    // Invalid multiplication: gop_length is NOT a multiple of intra_pic_rate
    test_profile["encoded_output_streams"][0]["encoding"]["encoding"]["hailo_encoder"]["rate_control"]["gop_length"] = 100;
    test_profile["encoded_output_streams"][0]["encoding"]["encoding"]["hailo_encoder"]["rate_control"]["intra_pic_rate"] = 30;
    result = rule.evaluate(test_profile);
    EXPECT_FALSE(result.passed) << "Rule should fail when gop_length (100) is not a multiple of intra_pic_rate (30)";
    EXPECT_EQ(result.error_violations.size(), 1) << "There should be one error violation";

    // Missing gop_length field
    test_profile = get_flattend_profile_json();
    test_profile["encoded_output_streams"][0]["encoding"]["encoding"]["hailo_encoder"]["rate_control"].erase("gop_length");
    result = rule.evaluate(test_profile);
    EXPECT_TRUE(result.passed) << "Rule should pass (with info) when gop_length is missing. errors: " << result.to_string();
    EXPECT_EQ(result.info_violations.size(), 0) << "There should be zero info violation";
}

// RateControlPqHdrConstraintRule Tests
class RateControlPqHdrConstraintRuleTest : public ::testing::Test {
protected:
    RateControlPqHdrConstraintRule rule;
};

TEST_F(RateControlPqHdrConstraintRuleTest, QpHdrConstraintTests) {
    if (rule.priority() == RulePriority::SKIP) {
        GTEST_SKIP() << "RateControlPqHdrConstraintRule is set to SKIP priority, skipping tests.";
    }
    nlohmann::json test_profile = get_flattend_profile_json();
    
    // Valid example profile
    auto result = rule.evaluate(test_profile);
    EXPECT_TRUE(result.passed) << "Rule should pass with valid example profile. errors: " << result.to_string();
    
    // Valid qp_hdr within range for CVBR mode
    test_profile["encoded_output_streams"][0]["encoding"]["encoding"]["hailo_encoder"]["rate_control"]["rc_mode"] = "CVBR";
    test_profile["encoded_output_streams"][0]["encoding"]["encoding"]["hailo_encoder"]["rate_control"]["quantization"]["qp_hdr"] = 25;
    test_profile["encoded_output_streams"][0]["encoding"]["encoding"]["hailo_encoder"]["rate_control"]["quantization"]["qp_min"] = 20;
    test_profile["encoded_output_streams"][0]["encoding"]["encoding"]["hailo_encoder"]["rate_control"]["quantization"]["qp_max"] = 30;
    result = rule.evaluate(test_profile);
    EXPECT_TRUE(result.passed) << "Rule should pass when qp_hdr (25) is within [qp_min (20), qp_max (30)]. errors: " << result.to_string();
    EXPECT_EQ(result.error_violations.size(), 0) << "There should be no error violations";

    // Valid qp_hdr = -1 (disabled)
    test_profile["encoded_output_streams"][0]["encoding"]["encoding"]["hailo_encoder"]["rate_control"]["rc_mode"] = "CVBR";
    test_profile["encoded_output_streams"][0]["encoding"]["encoding"]["hailo_encoder"]["rate_control"]["quantization"]["qp_hdr"] = -1;
    test_profile["encoded_output_streams"][0]["encoding"]["encoding"]["hailo_encoder"]["rate_control"]["quantization"]["qp_min"] = 20;
    test_profile["encoded_output_streams"][0]["encoding"]["encoding"]["hailo_encoder"]["rate_control"]["quantization"]["qp_max"] = 30;
    result = rule.evaluate(test_profile);
    EXPECT_TRUE(result.passed) << "Rule should pass when qp_hdr is -1 (disabled). errors: " << result.to_string();
    EXPECT_EQ(result.error_violations.size(), 0) << "There should be no error violations";

    // Invalid qp_hdr below qp_min
    test_profile["encoded_output_streams"][0]["encoding"]["encoding"]["hailo_encoder"]["rate_control"]["rc_mode"] = "CVBR";
    test_profile["encoded_output_streams"][0]["encoding"]["encoding"]["hailo_encoder"]["rate_control"]["quantization"]["qp_hdr"] = 15;
    test_profile["encoded_output_streams"][0]["encoding"]["encoding"]["hailo_encoder"]["rate_control"]["quantization"]["qp_min"] = 20;
    test_profile["encoded_output_streams"][0]["encoding"]["encoding"]["hailo_encoder"]["rate_control"]["quantization"]["qp_max"] = 30;
    result = rule.evaluate(test_profile);
    EXPECT_FALSE(result.passed) << "Rule should fail when qp_hdr (15) is below qp_min (20)";
    EXPECT_EQ(result.error_violations.size(), 1) << "There should be one error violation";

    // Invalid qp_hdr above qp_max
    test_profile["encoded_output_streams"][0]["encoding"]["encoding"]["hailo_encoder"]["rate_control"]["rc_mode"] = "CQP";
    test_profile["encoded_output_streams"][0]["encoding"]["encoding"]["hailo_encoder"]["rate_control"]["quantization"]["qp_hdr"] = 35;
    test_profile["encoded_output_streams"][0]["encoding"]["encoding"]["hailo_encoder"]["rate_control"]["quantization"]["qp_min"] = 20;
    test_profile["encoded_output_streams"][0]["encoding"]["encoding"]["hailo_encoder"]["rate_control"]["quantization"]["qp_max"] = 30;
    result = rule.evaluate(test_profile);
    EXPECT_FALSE(result.passed) << "Rule should fail when qp_hdr (35) is above qp_max (30)";
    EXPECT_EQ(result.error_violations.size(), 1) << "There should be one error violation";

    // VBR mode validation with valid qp_hdr
    test_profile["encoded_output_streams"][0]["encoding"]["encoding"]["hailo_encoder"]["rate_control"]["rc_mode"] = "VBR";
    test_profile["encoded_output_streams"][0]["encoding"]["encoding"]["hailo_encoder"]["rate_control"]["quantization"]["qp_hdr"] = 25;
    test_profile["encoded_output_streams"][0]["encoding"]["encoding"]["hailo_encoder"]["rate_control"]["quantization"]["qp_min"] = 20;
    test_profile["encoded_output_streams"][0]["encoding"]["encoding"]["hailo_encoder"]["rate_control"]["quantization"]["qp_max"] = 30;
    result = rule.evaluate(test_profile);
    EXPECT_TRUE(result.passed) << "Rule should pass for VBR mode with valid qp_hdr. errors: " << result.to_string();
}

