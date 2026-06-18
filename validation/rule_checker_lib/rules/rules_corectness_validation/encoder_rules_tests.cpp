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

// SmartEncoderEnablementRule Tests
class SmartEncoderEnablementRuleTest : public ::testing::Test {
protected:
    SmartEncoderEnablementRule rule;

    void enable_smart_encoder(nlohmann::json &profile) {
        auto &enc = profile["encoded_output_streams"][0]["encoding"]["encoding"]["hailo_encoder"];
        enc["smart_encoder"]["enabled"] = true;
        enc["smart_encoder"]["background_qp_delta"] = 5;
        enc["smart_encoder"]["rois"] = nlohmann::json::array();
    }
};

TEST_F(SmartEncoderEnablementRuleTest, PassesWithNoSmartEncoder) {
    if (rule.priority() == RulePriority::SKIP) {
        GTEST_SKIP() << "SmartEncoderEnablementRule is set to SKIP priority, skipping tests.";
    }
    nlohmann::json test_profile = get_flattend_profile_json();
    auto result = rule.evaluate(test_profile);
    EXPECT_TRUE(result.passed) << "Rule should pass with valid example profile (no smart_encoder). errors: " << result.to_string();
    EXPECT_EQ(result.error_violations.size(), 0) << "There should be no error violations";
}

TEST_F(SmartEncoderEnablementRuleTest, PassesWithH264AndCvbr) {
    nlohmann::json test_profile = get_flattend_profile_json();
    enable_smart_encoder(test_profile);
    auto &enc = test_profile["encoded_output_streams"][0]["encoding"]["encoding"]["hailo_encoder"];
    enc["config"]["output_stream"]["codec"] = "CODEC_TYPE_H264";
    enc["rate_control"]["rc_mode"] = "CVBR";
    auto result = rule.evaluate(test_profile);
    EXPECT_TRUE(result.passed) << "Rule should pass when codec is H264 and rc_mode is CVBR. errors: " << result.to_string();
    EXPECT_EQ(result.error_violations.size(), 0) << "There should be no error violations";
}

TEST_F(SmartEncoderEnablementRuleTest, FailsWithHevcCodec) {
    nlohmann::json test_profile = get_flattend_profile_json();
    enable_smart_encoder(test_profile);
    auto &enc = test_profile["encoded_output_streams"][0]["encoding"]["encoding"]["hailo_encoder"];
    enc["config"]["output_stream"]["codec"] = "CODEC_TYPE_HEVC";
    enc["rate_control"]["rc_mode"] = "CVBR";
    auto result = rule.evaluate(test_profile);
    EXPECT_FALSE(result.passed) << "Rule should fail when codec is HEVC";
    EXPECT_EQ(result.error_violations.size(), 1) << "There should be one error violation";
}

TEST_F(SmartEncoderEnablementRuleTest, FailsWithVbrRcMode) {
    nlohmann::json test_profile = get_flattend_profile_json();
    enable_smart_encoder(test_profile);
    auto &enc = test_profile["encoded_output_streams"][0]["encoding"]["encoding"]["hailo_encoder"];
    enc["config"]["output_stream"]["codec"] = "CODEC_TYPE_H264";
    enc["rate_control"]["rc_mode"] = "VBR";
    auto result = rule.evaluate(test_profile);
    EXPECT_FALSE(result.passed) << "Rule should fail when rc_mode is VBR";
    EXPECT_EQ(result.error_violations.size(), 1) << "There should be one error violation";
}

TEST_F(SmartEncoderEnablementRuleTest, FailsWithCbrRcMode) {
    nlohmann::json test_profile = get_flattend_profile_json();
    enable_smart_encoder(test_profile);
    auto &enc = test_profile["encoded_output_streams"][0]["encoding"]["encoding"]["hailo_encoder"];
    enc["config"]["output_stream"]["codec"] = "CODEC_TYPE_H264";
    enc["rate_control"]["rc_mode"] = "CBR";
    auto result = rule.evaluate(test_profile);
    EXPECT_FALSE(result.passed) << "Rule should fail when rc_mode is CBR";
    EXPECT_EQ(result.error_violations.size(), 1) << "There should be one error violation";
}

TEST_F(SmartEncoderEnablementRuleTest, PassesWhenDisabled) {
    nlohmann::json test_profile = get_flattend_profile_json();
    auto &enc = test_profile["encoded_output_streams"][0]["encoding"]["encoding"]["hailo_encoder"];
    enc["smart_encoder"]["enabled"] = false;
    enc["smart_encoder"]["background_qp_delta"] = 5;
    enc["smart_encoder"]["rois"] = nlohmann::json::array();
    enc["rate_control"]["rc_mode"] = "VBR";
    auto result = rule.evaluate(test_profile);
    EXPECT_TRUE(result.passed) << "Rule should pass when smart_encoder.enabled is false regardless of codec/rc_mode. errors: " << result.to_string();
    EXPECT_EQ(result.error_violations.size(), 0) << "There should be no error violations";
}

// SmartEncoderRoiBoundsRule Tests
class SmartEncoderRoiBoundsRuleTest : public ::testing::Test {
protected:
    SmartEncoderRoiBoundsRule rule;

    nlohmann::json make_profile_with_rois(const nlohmann::json &rois) {
        nlohmann::json profile = get_flattend_profile_json();
        auto &enc = profile["encoded_output_streams"][0]["encoding"]["encoding"]["hailo_encoder"];
        enc["smart_encoder"]["enabled"] = true;
        enc["smart_encoder"]["background_qp_delta"] = 5;
        enc["smart_encoder"]["rois"] = rois;
        return profile;
    }

    nlohmann::json make_roi(double x, double y, double w, double h) {
        return {{"x", x}, {"y", y}, {"width", w}, {"height", h}};
    }
};

TEST_F(SmartEncoderRoiBoundsRuleTest, PassesWithNoSmartEncoder) {
    if (rule.priority() == RulePriority::SKIP) {
        GTEST_SKIP() << "SmartEncoderRoiBoundsRule is set to SKIP priority, skipping tests.";
    }
    nlohmann::json test_profile = get_flattend_profile_json();
    auto result = rule.evaluate(test_profile);
    EXPECT_TRUE(result.passed) << "Rule should pass with valid example profile. errors: " << result.to_string();
    EXPECT_EQ(result.error_violations.size(), 0);
}

TEST_F(SmartEncoderRoiBoundsRuleTest, PassesWithSingleValidRoi) {
    if (rule.priority() == RulePriority::SKIP) {
        GTEST_SKIP() << "SmartEncoderRoiBoundsRule is set to SKIP priority, skipping tests.";
    }
    auto profile = make_profile_with_rois(nlohmann::json::array({make_roi(0.1, 0.1, 0.3, 0.3)}));
    auto result = rule.evaluate(profile);
    EXPECT_TRUE(result.passed) << "Rule should pass with valid ROI coordinates. errors: " << result.to_string();
    EXPECT_EQ(result.error_violations.size(), 0);
}

TEST_F(SmartEncoderRoiBoundsRuleTest, PassesAtUpperBoundary) {
    if (rule.priority() == RulePriority::SKIP) {
        GTEST_SKIP() << "SmartEncoderRoiBoundsRule is set to SKIP priority, skipping tests.";
    }
    auto profile = make_profile_with_rois(nlohmann::json::array({make_roi(0.2, 0.2, 0.8, 0.8)}));
    auto result = rule.evaluate(profile);
    EXPECT_TRUE(result.passed) << "Rule should pass when x+width and y+height equal 1.0. errors: " << result.to_string();
    EXPECT_EQ(result.error_violations.size(), 0);
}

TEST_F(SmartEncoderRoiBoundsRuleTest, PassesAtLowerBoundary) {
    if (rule.priority() == RulePriority::SKIP) {
        GTEST_SKIP() << "SmartEncoderRoiBoundsRule is set to SKIP priority, skipping tests.";
    }
    auto profile = make_profile_with_rois(nlohmann::json::array({make_roi(0.0, 0.0, 0.5, 0.5)}));
    auto result = rule.evaluate(profile);
    EXPECT_TRUE(result.passed) << "Rule should pass when x and y are 0.0. errors: " << result.to_string();
    EXPECT_EQ(result.error_violations.size(), 0);
}

TEST_F(SmartEncoderRoiBoundsRuleTest, FailsWithNegativeX) {
    if (rule.priority() == RulePriority::SKIP) {
        GTEST_SKIP() << "SmartEncoderRoiBoundsRule is set to SKIP priority, skipping tests.";
    }
    auto profile = make_profile_with_rois(nlohmann::json::array({make_roi(-0.1, 0.1, 0.3, 0.3)}));
    auto result = rule.evaluate(profile);
    EXPECT_FALSE(result.passed) << "Rule should fail when x is negative";
    EXPECT_EQ(result.error_violations.size(), 1);
}

TEST_F(SmartEncoderRoiBoundsRuleTest, FailsWithYAboveOne) {
    if (rule.priority() == RulePriority::SKIP) {
        GTEST_SKIP() << "SmartEncoderRoiBoundsRule is set to SKIP priority, skipping tests.";
    }
    auto profile = make_profile_with_rois(nlohmann::json::array({make_roi(0.1, 1.1, 0.3, 0.3)}));
    auto result = rule.evaluate(profile);
    EXPECT_FALSE(result.passed) << "Rule should fail when y is above 1.0";
    EXPECT_EQ(result.error_violations.size(), 1);
}

TEST_F(SmartEncoderRoiBoundsRuleTest, FailsWithWidthAboveOne) {
    if (rule.priority() == RulePriority::SKIP) {
        GTEST_SKIP() << "SmartEncoderRoiBoundsRule is set to SKIP priority, skipping tests.";
    }
    auto profile = make_profile_with_rois(nlohmann::json::array({make_roi(0.1, 0.1, 1.1, 0.3)}));
    auto result = rule.evaluate(profile);
    EXPECT_FALSE(result.passed) << "Rule should fail when width is above 1.0";
    EXPECT_EQ(result.error_violations.size(), 1);
}

TEST_F(SmartEncoderRoiBoundsRuleTest, FailsWithNegativeHeight) {
    if (rule.priority() == RulePriority::SKIP) {
        GTEST_SKIP() << "SmartEncoderRoiBoundsRule is set to SKIP priority, skipping tests.";
    }
    auto profile = make_profile_with_rois(nlohmann::json::array({make_roi(0.1, 0.1, 0.3, -0.1)}));
    auto result = rule.evaluate(profile);
    EXPECT_FALSE(result.passed) << "Rule should fail when height is negative";
    EXPECT_EQ(result.error_violations.size(), 1);
}

TEST_F(SmartEncoderRoiBoundsRuleTest, FailsWhenXPlusWidthExceedsOne) {
    if (rule.priority() == RulePriority::SKIP) {
        GTEST_SKIP() << "SmartEncoderRoiBoundsRule is set to SKIP priority, skipping tests.";
    }
    auto profile = make_profile_with_rois(nlohmann::json::array({make_roi(0.6, 0.1, 0.5, 0.3)}));
    auto result = rule.evaluate(profile);
    EXPECT_FALSE(result.passed) << "Rule should fail when x + width exceeds 1.0";
    EXPECT_EQ(result.error_violations.size(), 1);
}

TEST_F(SmartEncoderRoiBoundsRuleTest, FailsWhenYPlusHeightExceedsOne) {
    if (rule.priority() == RulePriority::SKIP) {
        GTEST_SKIP() << "SmartEncoderRoiBoundsRule is set to SKIP priority, skipping tests.";
    }
    auto profile = make_profile_with_rois(nlohmann::json::array({make_roi(0.1, 0.6, 0.3, 0.5)}));
    auto result = rule.evaluate(profile);
    EXPECT_FALSE(result.passed) << "Rule should fail when y + height exceeds 1.0";
    EXPECT_EQ(result.error_violations.size(), 1);
}

TEST_F(SmartEncoderRoiBoundsRuleTest, PassesWithMultipleValidRois) {
    if (rule.priority() == RulePriority::SKIP) {
        GTEST_SKIP() << "SmartEncoderRoiBoundsRule is set to SKIP priority, skipping tests.";
    }
    auto profile = make_profile_with_rois(nlohmann::json::array({make_roi(0.1, 0.1, 0.2, 0.2), make_roi(0.6, 0.6, 0.2, 0.2)}));
    auto result = rule.evaluate(profile);
    EXPECT_TRUE(result.passed) << "Rule should pass with multiple valid ROIs. errors: " << result.to_string();
    EXPECT_EQ(result.error_violations.size(), 0);
}

TEST_F(SmartEncoderRoiBoundsRuleTest, FailsWithOneInvalidOfMultipleRois) {
    if (rule.priority() == RulePriority::SKIP) {
        GTEST_SKIP() << "SmartEncoderRoiBoundsRule is set to SKIP priority, skipping tests.";
    }
    auto profile = make_profile_with_rois(nlohmann::json::array({make_roi(0.1, 0.1, 0.2, 0.2), make_roi(1.1, 0.6, 0.2, 0.2)}));
    auto result = rule.evaluate(profile);
    EXPECT_FALSE(result.passed) << "Rule should fail when one of multiple ROIs is invalid";
    EXPECT_EQ(result.error_violations.size(), 1);
}

// H264BaselineNoBFramesRule Tests
class H264BaselineNoBFramesRuleTest : public ::testing::Test {
protected:
    H264BaselineNoBFramesRule rule;
};

TEST_F(H264BaselineNoBFramesRuleTest, PassesWithValidExampleProfile) {
    if (rule.priority() == RulePriority::SKIP) {
        GTEST_SKIP() << "H264BaselineNoBFramesRule is set to SKIP priority, skipping tests.";
    }
    nlohmann::json test_profile = get_flattend_profile_json();
    auto result = rule.evaluate(test_profile);
    EXPECT_TRUE(result.passed) << "Rule should pass with valid example profile. errors: " << result.to_string();
}

TEST_F(H264BaselineNoBFramesRuleTest, PassesWithNoProfileSet) {
    nlohmann::json test_profile = get_flattend_profile_json();
    // Ensure profile is not set (default state)
    auto &enc = test_profile["encoded_output_streams"][0]["encoding"]["encoding"]["hailo_encoder"];
    enc["config"]["output_stream"].erase("profile");
    enc["gop_config"]["gop_size"] = 4;
    auto result = rule.evaluate(test_profile);
    EXPECT_TRUE(result.passed) << "Rule should pass when profile is not set. errors: " << result.to_string();
    EXPECT_EQ(result.error_violations.size(), 0);
}

TEST_F(H264BaselineNoBFramesRuleTest, PassesWithBaselineAndGopSize1) {
    nlohmann::json test_profile = get_flattend_profile_json();
    auto &enc = test_profile["encoded_output_streams"][0]["encoding"]["encoding"]["hailo_encoder"];
    enc["config"]["output_stream"]["profile"] = "VCENC_H264_BASE_PROFILE";
    enc["gop_config"]["gop_size"] = 1;
    auto result = rule.evaluate(test_profile);
    EXPECT_TRUE(result.passed) << "Rule should pass with Baseline profile and gop_size=1. errors: " << result.to_string();
    EXPECT_EQ(result.error_violations.size(), 0);
}

TEST_F(H264BaselineNoBFramesRuleTest, FailsWithBaselineAndGopSizeGreaterThan1) {
    nlohmann::json test_profile = get_flattend_profile_json();
    auto &enc = test_profile["encoded_output_streams"][0]["encoding"]["encoding"]["hailo_encoder"];
    enc["config"]["output_stream"]["profile"] = "VCENC_H264_BASE_PROFILE";
    enc["gop_config"]["gop_size"] = 2;
    auto result = rule.evaluate(test_profile);
    EXPECT_FALSE(result.passed) << "Rule should fail with Baseline profile and gop_size=2";
    EXPECT_EQ(result.error_violations.size(), 1);
}

TEST_F(H264BaselineNoBFramesRuleTest, PassesWithMainProfileAndGopSizeGreaterThan1) {
    nlohmann::json test_profile = get_flattend_profile_json();
    auto &enc = test_profile["encoded_output_streams"][0]["encoding"]["encoding"]["hailo_encoder"];
    enc["config"]["output_stream"]["profile"] = "VCENC_H264_MAIN_PROFILE";
    enc["gop_config"]["gop_size"] = 4;
    auto result = rule.evaluate(test_profile);
    EXPECT_TRUE(result.passed) << "Rule should pass with Main profile and gop_size>1. errors: " << result.to_string();
    EXPECT_EQ(result.error_violations.size(), 0);
}

TEST_F(H264BaselineNoBFramesRuleTest, PassesWithHighProfileAndGopSizeGreaterThan1) {
    nlohmann::json test_profile = get_flattend_profile_json();
    auto &enc = test_profile["encoded_output_streams"][0]["encoding"]["encoding"]["hailo_encoder"];
    enc["config"]["output_stream"]["profile"] = "VCENC_H264_HIGH_PROFILE";
    enc["gop_config"]["gop_size"] = 4;
    auto result = rule.evaluate(test_profile);
    EXPECT_TRUE(result.passed) << "Rule should pass with High profile and gop_size>1. errors: " << result.to_string();
    EXPECT_EQ(result.error_violations.size(), 0);
}

// ExclusiveEncoderTypeRule Tests
class ExclusiveEncoderTypeRuleTest : public ::testing::Test {
protected:
    ExclusiveEncoderTypeRule rule;
};

TEST_F(ExclusiveEncoderTypeRuleTest, PassesWithHailoEncoderOnly) {
    nlohmann::json test_profile = get_flattend_profile_json();
    auto result = rule.evaluate(test_profile);
    EXPECT_TRUE(result.passed) << "Rule should pass with hailo_encoder only. errors: " << result.to_string();
    EXPECT_EQ(result.error_violations.size(), 0);
}

TEST_F(ExclusiveEncoderTypeRuleTest, FailsWithBothEncoders) {
    nlohmann::json test_profile = get_flattend_profile_json();
    auto &encoding = test_profile["encoded_output_streams"][0]["encoding"]["encoding"];
    encoding["jpeg_encoder"] = {{"n_threads", 1}, {"quality", 85}};
    auto result = rule.evaluate(test_profile);
    EXPECT_FALSE(result.passed) << "Rule should fail when both hailo_encoder and jpeg_encoder are present";
    EXPECT_EQ(result.error_violations.size(), 1);
}

TEST_F(ExclusiveEncoderTypeRuleTest, FailsWithNoEncoder) {
    nlohmann::json test_profile = get_flattend_profile_json();
    auto &encoding = test_profile["encoded_output_streams"][0]["encoding"]["encoding"];
    encoding.erase("hailo_encoder");
    auto result = rule.evaluate(test_profile);
    EXPECT_FALSE(result.passed) << "Rule should fail when neither hailo_encoder nor jpeg_encoder is present";
    EXPECT_EQ(result.error_violations.size(), 1);
}

TEST_F(ExclusiveEncoderTypeRuleTest, PassesWithJpegEncoderOnly) {
    nlohmann::json test_profile = get_flattend_profile_json();
    auto &encoding = test_profile["encoded_output_streams"][0]["encoding"]["encoding"];
    encoding.erase("hailo_encoder");
    encoding["jpeg_encoder"] = {{"n_threads", 1}, {"quality", 85}};
    auto result = rule.evaluate(test_profile);
    EXPECT_TRUE(result.passed) << "Rule should pass with jpeg_encoder only. errors: " << result.to_string();
    EXPECT_EQ(result.error_violations.size(), 0);
}

