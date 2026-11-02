#pragma once

// Pure frame budget for Source engine traces.
// Does not call enginetrace, SetupBones, or ILuaBase, and does not start a thread.
// The number matches vrmod.utils.TraceBudget_MaxPerFrame (Lua is the tested contract).

static const int VRMOD_TRACE_BUDGET_MAX_PER_FRAME = 8;

struct VrmodTraceAdmit {
    int run;
    int deferred;
};

int vrmod_trace_budget_max_per_frame();

// Same rule as vrmod.utils.TraceBudget_Admit.
// Negative used, requested, or max => run 0.
// Negative requested => deferred 0. Otherwise a rejected used/max defers the request.
// When used and max are >= 0, used+run <= max if used <= max; if used is already
// over max, run is 0.
VrmodTraceAdmit vrmod_trace_budget_admit(int used, int requested, int max_per_frame);
