#include <gtest/gtest.h>
#include "eval_result.h"
#include "common.h"

using namespace rule_checker;

TEST(EvalResultTest, Merge) {
    EvalResult res1 = EvalResultBuilder::success();
    EvalResult res2 = EvalResultBuilder::error("rule1", "/path", "error msg");
    EvalResultBuilder::merge(res1, res2);
    EXPECT_FALSE(res1.passed);
    EXPECT_EQ(res1.error_violations.size(), 1);
    EXPECT_EQ(res1.info_violations.size(), 0);

    EvalResult res3 = EvalResultBuilder::info("rule2", "/path2", "info msg");
    EvalResultBuilder::merge(res1, res3);
    EXPECT_FALSE(res1.passed); // Still false from the error
    EXPECT_EQ(res1.error_violations.size(), 1);
    EXPECT_EQ(res1.info_violations.size(), 1);

    EvalResult res4 = EvalResultBuilder::success();
    EvalResultBuilder::merge(res1, res4);
    EXPECT_FALSE(res1.passed); // Still false
    EXPECT_EQ(res1.error_violations.size(), 1);
    EXPECT_EQ(res1.info_violations.size(), 1);
}

TEST(EvalResultTest, PromoteErrorsToInfo) {
    EvalResult res = EvalResultBuilder::error("rule1", "/path", "error msg");
    EvalResultBuilder::promote_errors_to_info(res);
    EXPECT_TRUE(res.passed);
    EXPECT_EQ(res.error_violations.size(), 0);
    EXPECT_EQ(res.info_violations.size(), 1);
    EXPECT_EQ(res.info_violations[0].rule_id, "rule1");
}
