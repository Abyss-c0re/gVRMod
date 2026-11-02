#include "trace_budget.h"

int vrmod_trace_budget_max_per_frame() {
    return VRMOD_TRACE_BUDGET_MAX_PER_FRAME;
}

VrmodTraceAdmit vrmod_trace_budget_admit(int used, int requested, int max_per_frame) {
    VrmodTraceAdmit out;
    out.run = 0;
    out.deferred = 0;
    if (requested < 0) {
        return out;
    }
    if (used < 0 || max_per_frame < 0) {
        out.deferred = requested;
        return out;
    }
    long long remain = (long long)max_per_frame - (long long)used;
    if (remain < 0) {
        remain = 0;
    }
    if (remain > (long long)requested) {
        remain = (long long)requested;
    }
    out.run = (int)remain;
    out.deferred = requested - out.run;
    return out;
}
