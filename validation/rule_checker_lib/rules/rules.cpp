#include "rules/encoder_rules.h"
#include "rules/profile_rules.h"
#include "rules/iq_rules.h"
#include "rule_checker.h"

namespace rule_checker {
    void RuleChecker::build_default_rule_engine()
    {
        rule_engine = RuleEngine();
        // Profile Rules
        rule_engine.add_rule(std::make_unique<ApplicationInputIstreams>());
        // IQ Rules
        rule_engine.add_rule(std::make_unique<GammaTableMonotonousRule>());
        rule_engine.add_rule(std::make_unique<ExclusiveAeAlgorithmRule>());
        rule_engine.add_rule(std::make_unique<DisableMultipleDenoiseAlgorithmsRule>());
        rule_engine.add_rule(std::make_unique<DisabledPostProcessingOnDenoiseDisabledRule>());
        rule_engine.add_rule(std::make_unique<ManualContrastAndBrightnessDuringHistogramEqualizationRule>());
        rule_engine.add_rule(std::make_unique<AdaptiveAePriorityRangesContinuousRule>());
        rule_engine.add_rule(std::make_unique<HdrAdaptiveAePriorityRangesContinuousRule>());
        rule_engine.add_rule(std::make_unique<DmscRangeCheckRule>());
        rule_engine.add_rule(std::make_unique<AlgorithmsArraySizeMatchRule>());
        // Encoder Rules
        rule_engine.add_rule(std::make_unique<MonitorFramesIntraPicRateMultiplicationRule>());
        rule_engine.add_rule(std::make_unique<GopLengthIntraPicRateMultiplicationRule>());
        rule_engine.add_rule(std::make_unique<RateControlPqHdrConstraintRule>());
        rule_engine.add_rule(std::make_unique<ProfileLevelConstraintRule>());
        rule_engine.add_rule(std::make_unique<PictureRcConstraintRule>());
        rule_engine.add_rule(std::make_unique<TargetBitrateWhenPictureRcRule>());
        rule_engine.add_rule(std::make_unique<SmartEncoderEnablementRule>());
        rule_engine.add_rule(std::make_unique<SmartEncoderRoiBoundsRule>());
    }
}