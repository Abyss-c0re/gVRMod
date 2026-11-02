#include "test_framework.h"
#include "trace/trace_budget.h"

TEST(TraceBudget_MaxIs8) {
    ASSERT_EQ(vrmod_trace_budget_max_per_frame(), 8);
    ASSERT_EQ(VRMOD_TRACE_BUDGET_MAX_PER_FRAME, 8);
}

TEST(TraceBudget_AdmitFitsAndSplits) {
    VrmodTraceAdmit a = vrmod_trace_budget_admit(0, 3, 8);
    ASSERT_EQ(a.run, 3);
    ASSERT_EQ(a.deferred, 0);

    a = vrmod_trace_budget_admit(6, 5, 8);
    ASSERT_EQ(a.run, 2);
    ASSERT_EQ(a.deferred, 3);
    ASSERT_TRUE(6 + a.run <= 8);

    a = vrmod_trace_budget_admit(8, 4, 8);
    ASSERT_EQ(a.run, 0);
    ASSERT_EQ(a.deferred, 4);

    a = vrmod_trace_budget_admit(10, 2, 8);
    ASSERT_EQ(a.run, 0);
    ASSERT_EQ(a.deferred, 2);

    a = vrmod_trace_budget_admit(0, 100, 8);
    ASSERT_EQ(a.run, 8);
    ASSERT_EQ(a.deferred, 92);

    a = vrmod_trace_budget_admit(0, 0, 8);
    ASSERT_EQ(a.run, 0);
    ASSERT_EQ(a.deferred, 0);

    a = vrmod_trace_budget_admit(0, 5, 0);
    ASSERT_EQ(a.run, 0);
    ASSERT_EQ(a.deferred, 5);
}

TEST(TraceBudget_AdmitNegativeRunsZero) {
    VrmodTraceAdmit a = vrmod_trace_budget_admit(-1, 4, 8);
    ASSERT_EQ(a.run, 0);
    ASSERT_EQ(a.deferred, 4);

    a = vrmod_trace_budget_admit(0, -3, 8);
    ASSERT_EQ(a.run, 0);
    ASSERT_EQ(a.deferred, 0);

    a = vrmod_trace_budget_admit(1, 4, -8);
    ASSERT_EQ(a.run, 0);
    ASSERT_EQ(a.deferred, 4);

    a = vrmod_trace_budget_admit(-2, -2, -2);
    ASSERT_EQ(a.run, 0);
    ASSERT_EQ(a.deferred, 0);
}
