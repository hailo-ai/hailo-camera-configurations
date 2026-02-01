#include <gtest/gtest.h>
#include "eval_result.h"
#include "common.h"
#include "iq_rules.h"
#include "common.h"

using namespace rule_checker;

// GammaTableMonotonousRule Tests
class GammaTableMonotonousRuleTest : public ::testing::Test {
protected:
    GammaTableMonotonousRule rule;
};

TEST_F(GammaTableMonotonousRuleTest, MonotonousGammaCurve) {
    if (rule.priority() == RulePriority::SKIP) {
        GTEST_SKIP() << "DmscRangeCheckRule is set to SKIP priority, skipping tests.";
    }
    nlohmann::json test_profile = get_flattend_profile_json();
    auto result = rule.evaluate(test_profile);
    EXPECT_TRUE(result.passed) << "Rule should pass with valid example profile. errors: " << result.to_string();
    
    test_profile["iq_settings"]["automatic_algorithms"]["AGamma64"]["curve"] = {{0, 10, 20, 30}, {5, 15, 25, 35}};
    result = rule.evaluate(test_profile);
    EXPECT_TRUE(result.passed) << "Rule should pass with monotonous gamma curve. errors: " << result.to_string();
    EXPECT_EQ(result.error_violations.size(), 0) << "There should be no error violations";

    test_profile["iq_settings"]["automatic_algorithms"]["AGamma64"]["curve"] = {{0, 30, 20, 40}, {5, 15, 25, 35}};
    result = rule.evaluate(test_profile);
    EXPECT_FALSE(result.passed) << "Rule should fail with non-monotonous gamma curve";
    EXPECT_EQ(result.error_violations.size(), 1) << "There should be one error violation";

    test_profile["iq_settings"]["automatic_algorithms"].erase("AGamma64");
    result = rule.evaluate(test_profile);
    EXPECT_TRUE(result.passed) << "Rule should pass (info) when gamma curve is missing. errors: " << result.to_string();
    EXPECT_EQ(result.info_violations.size(), 1) << "There should be at least one info violation";
}

// ExclusiveAeAlgorithmRule Tests
class ExclusiveAeAlgorithmRuleTest : public ::testing::Test {
protected:
    ExclusiveAeAlgorithmRule rule;
};

TEST_F(ExclusiveAeAlgorithmRuleTest, ValidExampleProfile) {
    if (rule.priority() == RulePriority::SKIP) {
        GTEST_SKIP() << "DmscRangeCheckRule is set to SKIP priority, skipping tests.";
    }
    nlohmann::json test_profile = get_flattend_profile_json();
    auto result = rule.evaluate(test_profile);
    EXPECT_TRUE(result.passed) << "Rule should pass with valid example profile. errors: " << result.to_string();

    test_profile["iq_settings"]["automatic_algorithms"]["Aev1"]["enable"] = true;
    test_profile["iq_settings"]["automatic_algorithms"]["Aev2"]["enable"] = false;
    test_profile["iq_settings"]["automatic_algorithms"]["Aehdr"]["enable"] = false;
    test_profile["iq_settings"]["automatic_algorithms"]["AdaptiveAe"]["enable"] = false;
    test_profile["iq_settings"]["automatic_algorithms"]["HDRAdaptiveAE"]["enable"] = false;
    result = rule.evaluate(test_profile);
    EXPECT_TRUE(result.passed) << "Rule should pass with only one AE algorithm enabled. errors: " << result.to_string();
    EXPECT_EQ(result.error_violations.size(), 0) << "There should be no error violations";

    test_profile["iq_settings"]["automatic_algorithms"]["Aev1"]["enable"] = true;
    test_profile["iq_settings"]["automatic_algorithms"]["Aev2"]["enable"] = true;
    test_profile["iq_settings"]["automatic_algorithms"]["Aehdr"]["enable"] = false;
    test_profile["iq_settings"]["automatic_algorithms"]["AdaptiveAe"]["enable"] = false;
    test_profile["iq_settings"]["automatic_algorithms"]["HDRAdaptiveAE"]["enable"] = false;
    
    result = rule.evaluate(test_profile);
    EXPECT_FALSE(result.passed) << "Rule should fail with multiple AE algorithms enabled";
    EXPECT_EQ(result.error_violations.size(), 1) << "There should be one error violation";
}

// DisableMultipleDenoiseAlgorithmsRule Tests
class DisableMultipleDenoiseAlgorithmsRuleTest : public ::testing::Test {
protected:
    DisableMultipleDenoiseAlgorithmsRule rule;
};

TEST_F(DisableMultipleDenoiseAlgorithmsRuleTest, ValidExampleProfile) {
    if (rule.priority() == RulePriority::SKIP) {
        GTEST_SKIP() << "DmscRangeCheckRule is set to SKIP priority, skipping tests.";
    }
    nlohmann::json test_profile = get_flattend_profile_json();
    auto result = rule.evaluate(test_profile);
    EXPECT_TRUE(result.passed) << "Rule should pass with valid example profile. errors: " << result.to_string();

    test_profile["iq_settings"]["denoise"]["enabled"] = true;
    test_profile["iq_settings"]["denoise"]["bayer"] = true;
    test_profile["iq_settings"]["automatic_algorithms"]["ADpf"]["enable"] = true;
    
    result = rule.evaluate(test_profile);
    EXPECT_TRUE(result.passed) << "Rule should pass when bayer denoise is enabled. errors: " << result.to_string();

    test_profile["iq_settings"]["denoise"]["enabled"] = true;
    test_profile["iq_settings"]["denoise"]["bayer"] = false;
    test_profile["iq_settings"]["automatic_algorithms"]["ADpf"]["enable"] = true;
    
    result = rule.evaluate(test_profile);
    EXPECT_FALSE(result.passed) << "Rule should fail when post ISP denoise is enabled with other denoise algorithms";
    EXPECT_EQ(result.error_violations.size(), 1) << "There should be one error violation";
}

// DisabledPostProcessingOnDenoiseDisabledRule Tests
class DisabledPostProcessingOnDenoiseDisabledRuleTest : public ::testing::Test {
protected:
    DisabledPostProcessingOnDenoiseDisabledRule rule;
};

TEST_F(DisabledPostProcessingOnDenoiseDisabledRuleTest, ValidExampleProfile) {
    if (rule.priority() == RulePriority::SKIP) {
        GTEST_SKIP() << "DmscRangeCheckRule is set to SKIP priority, skipping tests.";
    }
    nlohmann::json test_profile = get_flattend_profile_json();
    auto result = rule.evaluate(test_profile);
    EXPECT_TRUE(result.passed) << "Rule should pass with valid example profile. errors: " << result.to_string();

    test_profile["iq_settings"]["denoise"]["enabled"] = false;
    test_profile["iq_settings"]["automatic_algorithms"]["ACprocPost"]["auto_luma"] = true;

    result = rule.evaluate(test_profile);
    EXPECT_FALSE(result.passed) << "Rule should fail when denoise is disabled but auto_luma is enabled";
    EXPECT_EQ(result.error_violations.size(), 1) << "There should be one error violation";

    test_profile["iq_settings"]["denoise"]["enabled"] = false;
    test_profile["iq_settings"]["automatic_algorithms"]["ACprocPost"]["auto_luma"] = false;
    
    result = rule.evaluate(test_profile);
    EXPECT_TRUE(result.passed) << "Rule should pass when both denoise and auto_luma are disabled. errors: " << result.to_string();
}

// ManualContrastAndBrightnessDuringHistogramEqualizationRule Tests
class ManualContrastAndBrightnessDuringHistogramEqualizationRuleTest : public ::testing::Test {
protected:
    ManualContrastAndBrightnessDuringHistogramEqualizationRule rule;
};

TEST_F(ManualContrastAndBrightnessDuringHistogramEqualizationRuleTest, ManualContrastAndBrightnessTests) {
    if (rule.priority() == RulePriority::SKIP) {
        GTEST_SKIP() << "DmscRangeCheckRule is set to SKIP priority, skipping tests.";
    }
    nlohmann::json test_profile = get_flattend_profile_json();

    // Valid example profile
    auto result = rule.evaluate(test_profile);
    EXPECT_TRUE(result.passed) << "Rule should pass with valid example profile. errors: " << result.to_string();

    // Correct manual settings during histogram equalization
    test_profile["iq_settings"]["denoise"]["enabled"] = true;
    test_profile["iq_settings"]["automatic_algorithms"]["ACprocPost"]["auto_luma"] = false;
    test_profile["iq_settings"]["automatic_algorithms"]["ACprocPost"]["histogram_equalization"] = true;
    test_profile["iq_settings"]["automatic_algorithms"]["ACprocPost"]["manual_contrast"] = {1, 1, 1};
    test_profile["iq_settings"]["automatic_algorithms"]["ACprocPost"]["manual_brightness"] = {0, 0, 0};
    result = rule.evaluate(test_profile);
    EXPECT_TRUE(result.passed) << "Rule should pass with correct manual settings during histogram equalization. errors: " << result.to_string();

    // Incorrect manual contrast
    test_profile["iq_settings"]["automatic_algorithms"]["ACprocPost"]["manual_contrast"] = {1, 2, 1};
    test_profile["iq_settings"]["automatic_algorithms"]["ACprocPost"]["manual_brightness"] = {0, 0, 0};
    result = rule.evaluate(test_profile);
    EXPECT_FALSE(result.passed) << "Rule should fail with incorrect manual contrast. errors: " << result.to_string();
    EXPECT_EQ(result.error_violations.size(), 1) << "There should be one error violation";
}
// AdaptiveAePriorityRangesContinuousRule Tests
class AdaptiveAePriorityRangesContinuousRuleTest : public ::testing::Test {
protected:
    AdaptiveAePriorityRangesContinuousRule rule;
};

TEST_F(AdaptiveAePriorityRangesContinuousRuleTest, PriorityRangesTests) {
    if (rule.priority() == RulePriority::SKIP) {
        GTEST_SKIP() << "DmscRangeCheckRule is set to SKIP priority, skipping tests.";
    }
    nlohmann::json test_profile = get_flattend_profile_json();

    // Valid example profile
    auto result = rule.evaluate(test_profile);
    EXPECT_TRUE(result.passed) << "Rule should pass with valid example profile. errors: " << result.to_string();

    // Continuous priority ranges
    test_profile["iq_settings"]["automatic_algorithms"]["AdaptiveAe"]["enable"] = true;
    test_profile["iq_settings"]["automatic_algorithms"]["AdaptiveAe"]["priority"] = {
        {{0.0, 1.0}, {1, 2, 3}},
        {{1.0, 2.0}, {4, 5, 6}},
        {{2.0, 3.0}, {7, 8, 9}}
    };
    result = rule.evaluate(test_profile);
    EXPECT_TRUE(result.passed) << "Rule should pass with continuous priority ranges. errors: " << result.to_string();

    // Discontinuous priority ranges
    test_profile["iq_settings"]["automatic_algorithms"]["AdaptiveAe"]["priority"] = {
        {{0.0, 1.0}, {1, 2, 3}},
        {{1.5, 2.0}, {4, 5, 6}},
        {{2.0, 3.0}, {7, 8, 9}}
    };
    result = rule.evaluate(test_profile);
    EXPECT_FALSE(result.passed) << "Rule should fail with discontinuous priority ranges. errors: " << result.to_string();
    EXPECT_EQ(result.error_violations.size(), 1) << "There should be one error violation";
}
// HdrAdaptiveAePriorityRangesContinuousRule Tests
class HdrAdaptiveAePriorityRangesContinuousRuleTest : public ::testing::Test {
protected:
    HdrAdaptiveAePriorityRangesContinuousRule rule;
};

TEST_F(HdrAdaptiveAePriorityRangesContinuousRuleTest, PriorityRangesTests) {
    if (rule.priority() == RulePriority::SKIP) {
        GTEST_SKIP() << "DmscRangeCheckRule is set to SKIP priority, skipping tests.";
    }
    nlohmann::json test_profile = get_flattend_profile_json();

    // Valid example profile
    auto result = rule.evaluate(test_profile);
    EXPECT_TRUE(result.passed) << "Rule should pass with valid example profile. errors: " << result.to_string();

    // Continuous priority ranges
    test_profile["iq_settings"]["automatic_algorithms"]["HDRAdaptiveAE"]["enable"] = true;
    test_profile["iq_settings"]["automatic_algorithms"]["HDRAdaptiveAE"]["priority"] = {
        {{0.0, 1.0}, {1, 2, 3}},
        {{1.0, 2.0}, {4, 5, 6}},
        {{2.0, 3.0}, {7, 8, 9}}
    };
    result = rule.evaluate(test_profile);
    EXPECT_TRUE(result.passed) << "Rule should pass with continuous priority ranges. errors: " << result.to_string();

    // Discontinuous priority ranges
    test_profile["iq_settings"]["automatic_algorithms"]["HDRAdaptiveAE"]["priority"] = {
        {{0.0, 1.0}, {1, 2, 3}},
        {{1.5, 2.0}, {4, 5, 6}},
        {{2.0, 3.0}, {7, 8, 9}}
    };
    result = rule.evaluate(test_profile);
    EXPECT_FALSE(result.passed) << "Rule should fail with discontinuous priority ranges. errors: " << result.to_string();
    EXPECT_EQ(result.error_violations.size(), 1) << "There should be one error violation";
}
// DmscRangeCheckRule Tests
class DmscRangeCheckRuleTest : public ::testing::Test {
protected:
    DmscRangeCheckRule rule;
};

TEST_F(DmscRangeCheckRuleTest, DmscRangeTests) {
    if (rule.priority() == RulePriority::SKIP) {
        GTEST_SKIP() << "DmscRangeCheckRule is set to SKIP priority, skipping tests.";
    }
    nlohmann::json test_profile = get_flattend_profile_json();

    // Valid example profile
    auto result = rule.evaluate(test_profile);
    EXPECT_TRUE(result.passed) << "Rule should pass with valid example profile. errors: " << result.to_string();

    // Valid DMSC ranges
    test_profile["iq_settings"]["automatic_algorithms"]["ADmscv2"]["enable"] = true;
    test_profile["iq_settings"]["automatic_algorithms"]["ADmscv2"]["dmsc_sharpen_t1"] = {10};
    test_profile["iq_settings"]["automatic_algorithms"]["ADmscv2"]["dmsc_sharpen_t2_shift"] = {2};
    test_profile["iq_settings"]["automatic_algorithms"]["ADmscv2"]["dmsc_sharpen_t3"] = {100};
    test_profile["iq_settings"]["automatic_algorithms"]["ADmscv2"]["dmsc_sharpen_t4_shift"] = {3};
    result = rule.evaluate(test_profile);
    EXPECT_TRUE(result.passed) << "Rule should pass with valid DMSC ranges. errors: " << result.to_string();

    // Invalid DMSC ranges
    test_profile["iq_settings"]["automatic_algorithms"]["ADmscv2"]["dmsc_sharpen_t1"] = {100};
    test_profile["iq_settings"]["automatic_algorithms"]["ADmscv2"]["dmsc_sharpen_t2_shift"] = {2};
    test_profile["iq_settings"]["automatic_algorithms"]["ADmscv2"]["dmsc_sharpen_t3"] = {100};
    test_profile["iq_settings"]["automatic_algorithms"]["ADmscv2"]["dmsc_sharpen_t4_shift"] = {3};
    result = rule.evaluate(test_profile);
    EXPECT_FALSE(result.passed) << "Rule should fail with invalid DMSC ranges (t2 >= t3). errors: " << result.to_string();
    EXPECT_EQ(result.error_violations.size(), 1) << "There should be one error violation";
}

// AlgorithmsArraySizeMatchRule Tests
class AlgorithmsArraySizeMatchRuleTest : public ::testing::Test {
protected:
    AlgorithmsArraySizeMatchRule rule;
};

TEST_F(AlgorithmsArraySizeMatchRuleTest, ArraySizeMatchTests) {
    if (rule.priority() == RulePriority::SKIP) {
        GTEST_SKIP() << "DmscRangeCheckRule is set to SKIP priority, skipping tests.";
    }
    nlohmann::json test_profile = get_flattend_profile_json();

    // Valid example profile
    auto result = rule.evaluate(test_profile);
    EXPECT_TRUE(result.passed) << "Rule should pass with valid example profile. errors: " << result.to_string();

    // Matching array sizes
    test_profile["iq_settings"]["automatic_algorithms"]["TestAlgorithm"]["enable"] = true;
    test_profile["iq_settings"]["automatic_algorithms"]["TestAlgorithm"]["gains"] = {1, 2, 3};
    test_profile["iq_settings"]["automatic_algorithms"]["TestAlgorithm"]["values"] = {4, 5, 6};
    result = rule.evaluate(test_profile);
    EXPECT_TRUE(result.passed) << "Rule should pass with matching array sizes. errors: " << result.to_string();

    // Mismatching array sizes
    test_profile["iq_settings"]["automatic_algorithms"]["TestAlgorithm"]["gains"] = {1, 2, 3};
    test_profile["iq_settings"]["automatic_algorithms"]["TestAlgorithm"]["values"] = {4, 5, 6, 7};
    result = rule.evaluate(test_profile);
    EXPECT_FALSE(result.passed) << "Rule should fail with mismatching array sizes. errors: " << result.to_string();
    EXPECT_EQ(result.error_violations.size(), 1) << "There should be one error violation";
}
