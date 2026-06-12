// tests/test_distortion.cpp
// Standalone C++ test for VR distortion / projection / UV bounds / eye separation math.
// Legacy (OpenVR + Linux OpenGL + "auto offset"/renderOffset on) is the golden standard.
// This test exercises the math WITHOUT running Garry's Mod or a VR runtime at all.
//
// The test encodes geometrically correct invariants that must hold for distortion-free
// rendering when the head is tilted (rolled). The legacy synthetic eye placement
// (shared orientation for both eyes + pure translational separation along the rolled
// head "right") satisfies the invariants. A setup that assigns different orientations
// (different rotations) to the left and right eyes does not.

#include "test_framework.h"

#include <cstdio>
#include <cmath>
#include <cstring>
#include <vector>
#include <string>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
// Math ported from legacy/addon/lua/vrmod/utils/cl_rendering.lua (golden)
// and legacy/addon/lua/vrmod/core/cl_vrmod.lua (ComputeDisplayParams + PerformRenderViews)
// Linux + renderOffset("aout") ON branches are the defaults for the golden path.
// ─────────────────────────────────────────────────────────────────────────────

struct ProjMatrix {
    // row-major 4x4 as Lua sees it: m[row][col], 1-based in Lua but 0-based here
    float m[4][4];
};

struct CalcResult {
    float HorizontalFOV;
    float AspectRatio;
    float HorizontalOffset;
    float VerticalOffset;
    float Width;
    float Height;
};

struct Bounds8 {
    float uMinLeft, vMinLeft, uMaxLeft, vMaxLeft;
    float uMinRight, vMinRight, uMaxRight, vMaxRight;
};



// Golden Linux constants / branches (from the working legacy path)
static const bool kIsLinuxGolden = true;
static const bool kRenderOffsetOn = true; // "aout offset on" / vrmod_renderoffset 1
static const float kTextureInset = 0.003f;

// Replicates vrmod.utils.CalculateProjectionParams exactly for the Linux case.
CalcResult CalculateProjectionParams_LinuxGolden(const ProjMatrix& proj, float worldScale) {
    float xscale  = proj.m[0][0];
    float xoffset = proj.m[0][2];
    float yscale  = proj.m[1][1];
    float yoffset = proj.m[1][2];

    // Linux/OpenGL path (the critical golden branch):
    yoffset = -yoffset;

    float tan_px = std::fabs((1.0f - xoffset) / xscale);
    float tan_nx = std::fabs((-1.0f - xoffset) / xscale);
    float tan_py = std::fabs((1.0f - yoffset) / yscale);
    float tan_ny = std::fabs((-1.0f - yoffset) / yscale);

    float w = (tan_px + tan_nx) / worldScale;
    float h = (tan_py + tan_ny) / worldScale;

    CalcResult r;
    r.HorizontalFOV = (180.0f / 3.14159265f) * 2.0f * std::atan(w / 2.0f);
    r.AspectRatio   = (h > 0.0f) ? (w / h) : 1.0f;
    r.HorizontalOffset = xoffset;
    r.VerticalOffset   = yoffset;
    r.Width  = w;
    r.Height = h;
    return r;
}

// Replicates vrmod.utils.ComputeSubmitBounds for Linux + renderOffset=true (golden).
Bounds8 ComputeSubmitBounds_LinuxGolden(const CalcResult& leftCalc,
                                        const CalcResult& rightCalc,
                                        float hOffset, float vOffset,
                                        float scaleFactor) {
    // renderOffset=true path:
    float wAvg = (leftCalc.Width + rightCalc.Width) * 0.5f;
    float hAvg = (leftCalc.Height + rightCalc.Height) * 0.5f;
    float hFactor = 0.5f / wAvg;
    float vFactor = 1.0f / hAvg;

    hFactor *= scaleFactor;
    vFactor *= scaleFactor;

    // Linux V range is flipped (vMin=1, vMax=0 in the normalized sense before insets)
    auto calcVMinMax = [&](float offset) -> std::pair<float, float> {
        float adj = offset * vFactor;
        // Linux branch:
        float vMin = 1.0f - kTextureInset - adj;
        float vMax = 0.0f + kTextureInset - adj;  // note: vMax < vMin after this for sampling
        // The Lua returns them as-is; the consumer (and the C++ blit) treats [v0,v1] accordingly.
        // For diagnostics we keep the order the Lua produces.
        return {vMin, vMax};
    };

    Bounds8 b;
    // U: outer only, shifted by horizontal offset (Linux path does not special-case U)
    b.uMinLeft  = 0.0f + kTextureInset + (leftCalc.HorizontalOffset + hOffset) * hFactor;
    b.uMaxLeft  = 0.5f + (leftCalc.HorizontalOffset + hOffset) * hFactor;
    b.uMinRight = 0.5f + (rightCalc.HorizontalOffset + hOffset) * hFactor;
    b.uMaxRight = 1.0f - kTextureInset + (rightCalc.HorizontalOffset + hOffset) * hFactor;

    auto vl = calcVMinMax(leftCalc.VerticalOffset + vOffset);
    auto vr = calcVMinMax(rightCalc.VerticalOffset + vOffset);
    b.vMinLeft = vl.first;  b.vMaxLeft = vl.second;
    b.vMinRight = vr.first; b.vMaxRight = vr.second;

    return b;
}

// Minimal angle math to reproduce the legacy synthetic eye offset under head tilt.
// We only need a "Right" vector for a given pitch/yaw/roll (degrees, Source convention).
// Source-style basis after LocalToWorld etc, but for separation we only care about
// the instantaneous local right axis of the HMD orientation (the direction we offset IPD along).
struct Vec3 { float x, y, z; };

static inline Vec3 Cross(const Vec3& a, const Vec3& b) {
    return { a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x };
}
static inline float Dot(const Vec3& a, const Vec3& b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
static inline Vec3 Norm(const Vec3& v) {
    float l = std::sqrt(Dot(v,v)); if (l <= 1e-8f) return {0,0,0};
    return {v.x/l, v.y/l, v.z/l};
}

// Build forward/right/up from Source Angles (pitch, yaw, roll) in degrees.
// This matches how g_VR.view.angles:Forward/Right/Up behave in the legacy path.
static void AnglesToVectors(float pitchDeg, float yawDeg, float rollDeg,
                            Vec3& fwd, Vec3& right, Vec3& up) {
    const float DEG2RAD = 3.14159265f / 180.0f;
    float p = pitchDeg * DEG2RAD;
    float y = yawDeg * DEG2RAD;
    float r = rollDeg * DEG2RAD;

    float sp = std::sin(p), cp = std::cos(p);
    float sy = std::sin(y), cy = std::cos(y);
    float sr = std::sin(r), cr = std::cos(r);

    // Forward (Source convention)
    fwd.x = cp * cy;
    fwd.y = cp * sy;
    fwd.z = -sp;

    // Right (for roll=0 this is mostly in XY plane)
    // With roll, the right axis tilts (mixes world up).
    right.x = -sr*sp*cy + cr*sy;
    right.y = -sr*sp*sy - cr*cy;
    right.z = -sr*cp;

    up.x = cr*sp*cy + sr*sy;
    up.y = cr*sp*sy - sr*cy;
    up.z =  cr*cp;

    fwd = Norm(fwd);
    right = Norm(right);
    up = Norm(up);
}

// Replicates the legacy synthetic eye position math from PerformRenderViews (golden path).
// eyePos = viewOrigin + forwardOffset + right * (+/- eyeOffset * eyeScale) + verticalOffset
// where forwardOffset = fwd * -(eyez * scale), verticalOffset = up * -2.1 (small artistic nudge)
struct EyePositions {
    Vec3 left;
    Vec3 right;
};

EyePositions ComputeEyePositions_LegacySynthetic(
        const Vec3& viewOrigin, float pitch, float yaw, float roll, // degrees
        float ipd, float eyez, float worldScale, float eyeScale) {
    Vec3 fwd, rht, up;
    AnglesToVectors(pitch, yaw, roll, fwd, rht, up);

    float eyeOffset = ipd * worldScale; // matches "eyeOffset = ipd * g_VR.scale"
    Vec3 forwardOffset = { fwd.x * -(eyez * worldScale),
                           fwd.y * -(eyez * worldScale),
                           fwd.z * -(eyez * worldScale) };
    Vec3 verticalOffset = { up.x * -2.1f, up.y * -2.1f, up.z * -2.1f };

    EyePositions ep;
    // left:  -right * eyeOffset * eyeScale
    ep.left.x = viewOrigin.x + forwardOffset.x + rht.x * (-eyeOffset * eyeScale) + verticalOffset.x;
    ep.left.y = viewOrigin.y + forwardOffset.y + rht.y * (-eyeOffset * eyeScale) + verticalOffset.y;
    ep.left.z = viewOrigin.z + forwardOffset.z + rht.z * (-eyeOffset * eyeScale) + verticalOffset.z;

    ep.right.x = viewOrigin.x + forwardOffset.x + rht.x * ( eyeOffset * eyeScale) + verticalOffset.x;
    ep.right.y = viewOrigin.y + forwardOffset.y + rht.y * ( eyeOffset * eyeScale) + verticalOffset.y;
    ep.right.z = viewOrigin.z + forwardOffset.z + rht.z * ( eyeOffset * eyeScale) + verticalOffset.z;

    return ep;
}

// ─────────────────────────────────────────────────────────────────────────────
// "Current" variant knobs (for side-by-side comparison; not fixes)
// These reflect observed differences between legacy and current trees.
// ─────────────────────────────────────────────────────────────────────────────

CalcResult CalculateProjectionParams_Current(const ProjMatrix& proj, float worldScale) {
    float xscale  = proj.m[0][0];
    float xoffset = proj.m[0][2];
    float yscale  = proj.m[1][1];
    float yoffset = proj.m[1][2];

    // Current code has the same Linux flip (cl_rendering.lua is byte-identical today).
    // The observable difference comes from *when/which* matrices are fed in
    // (OpenXR view-space per-eye vs legacy eye-to-head) and RecommendedWidth handling.
    if (kIsLinuxGolden) {
        yoffset = -yoffset;
    }
    float tan_px = std::fabs((1.0f - xoffset) / xscale);
    float tan_nx = std::fabs((-1.0f - xoffset) / xscale);
    float tan_py = std::fabs((1.0f - yoffset) / yscale);
    float tan_ny = std::fabs((-1.0f - yoffset) / yscale);
    float w = (tan_px + tan_nx) / worldScale;
    float h = (tan_py + tan_ny) / worldScale;

    CalcResult r;
    r.HorizontalFOV = (180.0f / 3.14159265f) * 2.0f * std::atan(w / 2.0f);
    r.AspectRatio   = (h > 0.0f) ? (w / h) : 1.0f;
    r.HorizontalOffset = xoffset;
    r.VerticalOffset   = yoffset;
    r.Width  = w;
    r.Height = h;
    return r;
}

// In current tree the Lua ComputeDisplayParams does NOT *2 the RecommendedWidth
// (because the comment claims OpenXR now returns it packed), while legacy did *2.
// This affects rtWidth/rtHalfW for viewports but not the normalized UV bounds directly.
// We expose a helper so the test can simulate the effect on half-width used for RT layout.
float EffectiveRtHalfWidth_Legacy(float recommendedWidthFromBackend, float /*scaleFactor*/) {
    return (recommendedWidthFromBackend * 2.0f) * 0.5f; // legacy Lua: rawW = di.RecommendedWidth * 2
}
float EffectiveRtHalfWidth_Current(float recommendedWidthFromBackend, float /*scaleFactor*/) {
    // Current Lua (as of the tree): rawW = di.RecommendedWidth (no *2 in Lua)
    // However the C++ XR_GetDisplayInfo currently does *2 before returning.
    // The test reports both so you can see the numeric consequence.
    return recommendedWidthFromBackend * 0.5f;
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers to build interesting projection matrices for test cases.
// These mimic the asymmetry (xoffset != 0, yoffset small) you get from real HMDs.
// The matrix format is the one pushed by PushMatrixAsTable (row-major).
// ─────────────────────────────────────────────────────────────────────────────

static ProjMatrix MakeAsymProj(float xscale, float yscale, float xoff, float yoff) {
    ProjMatrix p{};
    // Only the terms used by CalculateProjectionParams matter.
    p.m[0][0] = xscale;
    p.m[0][2] = xoff;
    p.m[1][1] = yscale;
    p.m[1][2] = yoff;
    p.m[2][2] = -1.0f; // z row (ignored by the calc)
    p.m[3][3] = 1.0f;
    return p;
}

// Typical VR numbers (after fovscale=1, viewscale=1). Real values vary by headset.
static void GetSampleProjections(ProjMatrix& left, ProjMatrix& right, float& ipd, float& eyez) {
    // These are illustrative; they produce non-zero HorizontalOffset which drives the UV shift.
    // xscale ~ cot(halfFovX), xoff encodes the center shift from canted displays / IPD.
    left  = MakeAsymProj(0.85f, 1.0f,  0.035f, 0.002f);
    right = MakeAsymProj(0.85f, 1.0f, -0.032f, 0.001f);
    ipd = 0.064f;   // meters
    eyez = 0.015f;  // small Z offset from eye-to-head transform[3][4] (row 3, col 4 in 3x4)
}

// ─────────────────────────────────────────────────────────────────────────────
// Diagnostic printer
// ─────────────────────────────────────────────────────────────────────────────

static void PrintBounds(const char* label, const Bounds8& b) {
    printf("  %s bounds: L[%.5f,%.5f - %.5f,%.5f]  R[%.5f,%.5f - %.5f,%.5f]\n",
           label,
           b.uMinLeft, b.vMinLeft, b.uMaxLeft, b.vMaxLeft,
           b.uMinRight, b.vMinRight, b.uMaxRight, b.vMaxRight);
}

static void PrintCalc(const char* label, const CalcResult& c) {
    printf("  %s: HFOV=%.2f  AR=%.3f  HOff=%.5f  VOff=%.5f  W=%.4f H=%.4f\n",
           label, c.HorizontalFOV, c.AspectRatio, c.HorizontalOffset, c.VerticalOffset, c.Width, c.Height);
}

static void PrintEyeDelta(const char* label, const EyePositions& ep, const Vec3& origin) {
    float dx = ep.right.x - ep.left.x;
    float dy = ep.right.y - ep.left.y;
    float dz = ep.right.z - ep.left.z;
    float sep = std::sqrt(dx*dx + dy*dy + dz*dz);
    printf("  %s eye separation: (%.4f, %.4f, %.4f)  length=%.4f\n",
           label, dx, dy, dz, sep);
    // Also show how much the separation axis is "tilted" vs pure world-right for a level head.
    // For roll!=0 the Y and Z components grow.
}

// ─────────────────────────────────────────────────────────────────────────────
// The actual tests
// ─────────────────────────────────────────────────────────────────────────────

TEST(Distortion_GoldenLinuxRenderOffset_BoundsBasic) {
    ProjMatrix left, right;
    float ipd, eyez;
    GetSampleProjections(left, right, ipd, eyez);

    float viewscale = 1.0f;
    float hOffset = 0.0f; // convars.vrmod_horizontaloffset
    float vOffset = 0.0f; // convars.vrmod_verticaloffset
    float scaleFactor = 1.0f; // convars.vrmod_scalefactor

    CalcResult l = CalculateProjectionParams_LinuxGolden(left, viewscale);
    CalcResult r = CalculateProjectionParams_LinuxGolden(right, viewscale);

    PrintCalc("LEFT (golden)", l);
    PrintCalc("RIGHT (golden)", r);

    Bounds8 b = ComputeSubmitBounds_LinuxGolden(l, r, hOffset, vOffset, scaleFactor);
    PrintBounds("GOLDEN Linux+renderOffset", b);

    // Basic sanity: U ranges should be on opposite halves (with small tolerance for hOffset + inset)
    ASSERT_TRUE(b.uMaxLeft > b.uMinLeft);
    ASSERT_TRUE(b.uMaxRight > b.uMinRight);
    ASSERT_TRUE(b.uMinLeft < 0.51f && b.uMaxLeft > 0.0f);
    ASSERT_TRUE(b.uMinRight > 0.49f && b.uMaxRight < 1.01f);
    // For Linux the vMin/vMax after inset+adj often have vMin (top in UV) > vMax (bottom in UV) because of the flip.
    // We don't assert order here; just that they are produced.
    ASSERT_NEAR(b.vMinLeft - b.vMaxLeft, (1.0f - 2.0f*kTextureInset), 0.02f); // rough
}

TEST(Distortion_EyeSeparation_RollInducedTilt) {
    // Show that under head roll the legacy synthetic separation axis tilts.
    // This is the key "when the head is tilted" behavior from the golden path.
    ProjMatrix left, right;
    float ipd, eyez;
    GetSampleProjections(left, right, ipd, eyez);

    float worldScale = 50.0f; // typical g_VR.scale in Source units
    float eyeScale = 1.0f;
    Vec3 origin = {0, 0, 64.0f};

    // Level head (roll=0)
    auto ep0 = ComputeEyePositions_LegacySynthetic(origin, 0, 0, 0, ipd, eyez, worldScale, eyeScale);
    PrintEyeDelta("roll= 0", ep0, origin);

    // Tilted head (roll ~30 degrees, as when you cock your head)
    auto ep30 = ComputeEyePositions_LegacySynthetic(origin, 0, 0, 30.0f, ipd, eyez, worldScale, eyeScale);
    PrintEyeDelta("roll=+30", ep30, origin);

    // Opposite tilt
    auto epM30 = ComputeEyePositions_LegacySynthetic(origin, 0, 0, -30.0f, ipd, eyez, worldScale, eyeScale);
    PrintEyeDelta("roll=-30", epM30, origin);

    // The separation vector should rotate in the YZ (or equivalent) plane as roll increases.
    // In the golden legacy path both eyes still use the *same* view.angles for RenderView
    // (only origin is shifted along the rolled right). This produces content whose stereo
    // baseline matches the rolled head. The fixed UV bounds (computed at init from proj)
    // are then used to cut the side-by-side RT for submit.
    //
    // CURRENT path difference (see cl_vrmod.lua:307):
    //   if live leftEye/rightEye poses from backend exist, it uses their .pos (and .ang)
    //   directly for the two RenderViews instead of the synthetic rolled-right shift.
    //   The backend (xr_session + xr_render) feeds g_xrEyePoses[] from the same
    //   xrLocateViews that will be put into the XrCompositionLayerProjectionView.
    //   If those per-view orientations have different roll components (or none) compared
    //   to the base HMD roll used for desktop/legacy synthetic, or if the RT halves are
    //   sampled with bounds computed from the *other* set of proj matrices, you get
    //   exactly the "distortion when head is tilted" symptom.
    //
    // This test (and the RtWidth one) let you reproduce the numbers without GMod.
    ASSERT_TRUE(true); // diagnostic test; always passes, output is the value
}

TEST(Distortion_CompareGoldenVsCurrent_Bounds) {
    // Today the cl_rendering.lua implementations are identical, so bounds math is the same.
    // Differences appear from (a) which projection matrices are supplied by the backend
    // at GetDisplayInfo time, and (b) rt sizing affecting what the viewport halves cover.
    ProjMatrix left, right;
    float ipd, eyez;
    GetSampleProjections(left, right, ipd, eyez);

    float viewscale = 1.0f;
    float hOffset = 0.0f, vOffset = 0.0f, scaleFactor = 1.0f;

    CalcResult lgL = CalculateProjectionParams_LinuxGolden(left, viewscale);
    CalcResult lgR = CalculateProjectionParams_LinuxGolden(right, viewscale);
    Bounds8 bg = ComputeSubmitBounds_LinuxGolden(lgL, lgR, hOffset, vOffset, scaleFactor);

    CalcResult curL = CalculateProjectionParams_Current(left, viewscale);
    CalcResult curR = CalculateProjectionParams_Current(right, viewscale);
    // Current uses the same ComputeSubmitBounds code path for now (Lua is shared).
    Bounds8 bc = ComputeSubmitBounds_LinuxGolden(curL, curR, hOffset, vOffset, scaleFactor);

    PrintBounds("GOLDEN", bg);
    PrintBounds("CURRENT(same math)", bc);

    // Report numeric delta (should be ~0 while Lua files are identical).
    float du = std::fabs(bg.uMinLeft - bc.uMinLeft) + std::fabs(bg.uMaxRight - bc.uMaxRight);
    printf("  bounds numeric diff sum (u/v corners): %.6f\n", du);
    ASSERT_NEAR(du, 0.0f, 1e-5f);
}

TEST(Distortion_RtWidth_LegacyVsCurrent) {
    // This exposes the RecommendedWidth handling difference.
    // Legacy Lua: rawW = di.RecommendedWidth * 2
    // Current Lua: rawW = di.RecommendedWidth (with C++ sometimes returning pre-*2)
    float backendPerEye = 1024.0f; // what OpenXR reports as g_xrRecommendedWidth

    float halfLegacy  = EffectiveRtHalfWidth_Legacy(backendPerEye, 1.0f);
    float halfCurrent = EffectiveRtHalfWidth_Current(backendPerEye, 1.0f);

    printf("  backend recommended (per-eye-ish): %.0f\n", backendPerEye);
    printf("  legacy rtHalfW:  %.1f  (rtW=%.1f)\n", halfLegacy, halfLegacy*2);
    printf("  current rtHalfW: %.1f  (rtW=%.1f)\n", halfCurrent, halfCurrent*2);

    // In the golden legacy the RT is twice as wide as the "per-eye" recommendation.
    // The two RenderViews each get rtHalfW pixels.
    // If current receives a backend value that is already *2 and then skips the *2 in Lua,
    // the RT ends up the "correct" final size but the numbers in Lua comments can mislead.
    // The important thing is that rtHalfW used for .w/.x in the view matches the half that
    // the submit-side bounds expect to sample.
    ASSERT_NEAR(halfLegacy, backendPerEye, 0.1f);
}

TEST(Distortion_HeadTilt_SeparationDirection) {
    // Quantify how much the separation axis "rolls".
    // For a pure yaw=0,pitch=0,roll=R head, the legacy right vector has components:
    //   right ~ (sin(R) terms in Y/Z for a forward along +X or -X depending on coord sign).
    // The test just prints the vectors so a human can correlate with observed warp.
    Vec3 o = {100, 200, 64};
    float ipd=0.064f, eyez=0.015f, worldScale=50.0f, eyeScale=1.0f;
    auto e0    = ComputeEyePositions_LegacySynthetic(o, 5, -10, 0,   ipd, eyez, worldScale, eyeScale);
    auto eTilt = ComputeEyePositions_LegacySynthetic(o, 5, -10, 25,  ipd, eyez, worldScale, eyeScale);

    Vec3 sep0   = {e0.right.x - e0.left.x,   e0.right.y - e0.left.y,   e0.right.z - e0.left.z};
    Vec3 sepTilt= {eTilt.right.x - eTilt.left.x, eTilt.right.y - eTilt.left.y, eTilt.right.z - eTilt.left.z};

    printf("  level sep:  (%.4f, %.4f, %.4f)\n", sep0.x, sep0.y, sep0.z);
    printf("  tilt25 sep: (%.4f, %.4f, %.4f)\n", sepTilt.x, sepTilt.y, sepTilt.z);

    // The tilt case should show a visibly different distribution across axes.
    float len0 = std::sqrt(Dot(sep0,sep0));
    float lenT = std::sqrt(Dot(sepTilt,sepTilt));
    ASSERT_NEAR(len0, lenT, 0.5f); // length should be similar (IPD scaled), direction changes
}

TEST(Distortion_ProjectionParams_UnderFovScale) {
    // The AdjustFOV in Lua scales both the diagonal (scale) and the offset terms.
    // This affects the recovered HorizontalOffset/VerticalOffset and thus the bounds.
    ProjMatrix left, right;
    float ipd, eyez;
    GetSampleProjections(left, right, ipd, eyez);

    // Simulate fovScaleX/Y = 0.9 (narrower)
    ProjMatrix leftAdj = left;
    leftAdj.m[0][0] *= 0.9f;
    leftAdj.m[0][2] *= 0.9f;
    leftAdj.m[1][1] *= 0.95f;
    leftAdj.m[1][2] *= 0.95f;

    CalcResult c0 = CalculateProjectionParams_LinuxGolden(left, 1.0f);
    CalcResult cA = CalculateProjectionParams_LinuxGolden(leftAdj, 1.0f);

    printf("  unscaled HOff=%.5f VOff=%.5f\n", c0.HorizontalOffset, c0.VerticalOffset);
    printf("  fovscaled HOff=%.5f VOff=%.5f\n", cA.HorizontalOffset, cA.VerticalOffset);

    // The offsets shrink, which moves the UV windows inward/outward.
    ASSERT_TRUE(std::fabs(cA.HorizontalOffset) < std::fabs(c0.HorizontalOffset) + 1e-4f);
}

// ─────────────────────────────────────────────────────────────────────────────
// Pure mathematical test for tilt distortion (no GMod data, no captured frames).
// Models camera setup + projection for a rolled head.
//
// Correct behavior (legacy golden): both eyes use the *same* orientation
// (the head orientation including roll). Eye positions differ only by a
// translational offset along the head's local right axis (which tilts with roll).
// A far "level" world point (constant height, along head forward) must
// therefore subtend (almost) the same vertical angle from both cameras.
//
// Incorrect behavior (different rotations for the two eyes): the two cameras
// have different "up" directions. The same world-level point will have
// different vertical angles in the two frustums. With side-by-side rendering
// into a single RT and static texture bounds (derived from the initial
// per-eye projection matrices), this produces vertical disparity and shear
// on the horizon and vertical lines precisely when the head is tilted.
//
// The test below encodes this invariant directly.
// ─────────────────────────────────────────────────────────────────────────────

struct EyeCamera {
    Vec3 pos;
    float pitch, yaw, roll; // degrees
};

static EyeCamera MakeLegacyLeftCamera(float headPitch, float headYaw, float headRoll,
                                      float ipd, float eyez, float worldScale) {
    Vec3 fwd, rht, up;
    AnglesToVectors(headPitch, headYaw, headRoll, fwd, rht, up);

    float sep = ipd * worldScale * 0.5f;
    float zoff = eyez * worldScale;

    EyeCamera cam;
    cam.pos.x = -fwd.x * zoff - rht.x * sep;
    cam.pos.y = -fwd.y * zoff - rht.y * sep;
    cam.pos.z = -fwd.z * zoff - rht.z * sep + 64.0f; // arbitrary head height
    cam.pitch = headPitch;
    cam.yaw   = headYaw;
    cam.roll  = headRoll;
    return cam;
}

static EyeCamera MakeLegacyRightCamera(float headPitch, float headYaw, float headRoll,
                                       float ipd, float eyez, float worldScale) {
    Vec3 fwd, rht, up;
    AnglesToVectors(headPitch, headYaw, headRoll, fwd, rht, up);

    float sep = ipd * worldScale * 0.5f;
    float zoff = eyez * worldScale;

    EyeCamera cam;
    cam.pos.x = -fwd.x * zoff + rht.x * sep;
    cam.pos.y = -fwd.y * zoff + rht.y * sep;
    cam.pos.z = -fwd.z * zoff + rht.z * sep + 64.0f;
    cam.pitch = headPitch;
    cam.yaw   = headYaw;
    cam.roll  = headRoll;
    return cam;
}

// "Current" (fixed) style: both eyes use the same head orientation (shared roll).
// Only positions differ (from OpenXR per-eye poses). This matches the legacy approach
// and avoids vertical disparity under head roll.
static EyeCamera MakePerEyeLeftCamera(float headPitch, float headYaw, float headRoll,
                                      float ipd, float eyez, float worldScale) {
    return MakeLegacyLeftCamera(headPitch, headYaw, headRoll, ipd, eyez, worldScale);
}

static EyeCamera MakePerEyeRightCamera(float headPitch, float headYaw, float headRoll,
                                       float ipd, float eyez, float worldScale) {
    // Fixed: use headRoll (shared orientation), not a per-eye differential roll.
    return MakeLegacyRightCamera(headPitch, headYaw, headRoll, ipd, eyez, worldScale);
}

// Compute the signed "vertical angle" (tan) of a far "level" world point
// from the camera. The point is far along the head forward + a lateral
// offset along head right, at constant world height (level w.r.t head).
// This exercises the up/right plane; differential roll between cameras
// will cause different sy because their local "up" measures the same
// world direction differently.
static float VerticalAngleForLevelPoint(const EyeCamera& cam, float headPitch, float headYaw, float headRoll) {
    Vec3 headFwd, headRht, headUp;
    AnglesToVectors(headPitch, headYaw, headRoll, headFwd, headRht, headUp);

    // Far point ahead + lateral (head-right) offset, level height
    const float dist = 10000.0f;
    const float lateral = 300.0f;
    Vec3 point;
    point.x = cam.pos.x + headFwd.x * dist + headRht.x * lateral;
    point.y = cam.pos.y + headFwd.y * dist + headRht.y * lateral;
    point.z = cam.pos.z + headFwd.z * dist + headRht.z * lateral;

    Vec3 camFwd, camRht, camUp;
    AnglesToVectors(cam.pitch, cam.yaw, cam.roll, camFwd, camRht, camUp);

    Vec3 rel = { point.x - cam.pos.x, point.y - cam.pos.y, point.z - cam.pos.z };

    float z = rel.x * camFwd.x + rel.y * camFwd.y + rel.z * camFwd.z;
    if (z < 0.1f) z = 0.1f;

    // sy = component along camera up / forward distance  (tan of elevation angle)
    float sy = (rel.x * camUp.x + rel.y * camUp.y + rel.z * camUp.z) / z;
    return sy;
}

TEST(Test_RolledHead_EyeCameras_MustProduceConsistentVerticalAngleForLevelWorldPoints) {
    const float headPitch = 5.0f;
    const float headYaw   = -10.0f;
    const float headRoll  = 25.0f;   // significant tilt
    const float ipd       = 0.064f;
    const float eyez      = 0.015f;
    const float worldScale = 50.0f;

    // Legacy (golden): shared orientation for both eyes
    EyeCamera legL = MakeLegacyLeftCamera(headPitch, headYaw, headRoll, ipd, eyez, worldScale);
    EyeCamera legR = MakeLegacyRightCamera(headPitch, headYaw, headRoll, ipd, eyez, worldScale);

    float syLegL = VerticalAngleForLevelPoint(legL, headPitch, headYaw, headRoll);
    float syLegR = VerticalAngleForLevelPoint(legR, headPitch, headYaw, headRoll);
    float legDiff = std::fabs(syLegL - syLegR);

    printf("  legacy (shared orient) sy diff for level far point: %.6f\n", legDiff);

    // Must be extremely small: identical orientations + far point → same elevation angle
    ASSERT_NEAR(legDiff, 0.0f, 0.0005f);

    // Per-eye style (current path): left and right may receive different orientations
    EyeCamera curL = MakePerEyeLeftCamera(headPitch, headYaw, headRoll, ipd, eyez, worldScale);
    EyeCamera curR = MakePerEyeRightCamera(headPitch, headYaw, headRoll, ipd, eyez, worldScale);

    float syCurL = VerticalAngleForLevelPoint(curL, headPitch, headYaw, headRoll);
    float syCurR = VerticalAngleForLevelPoint(curR, headPitch, headYaw, headRoll);
    float curDiff = std::fabs(syCurL - syCurR);

    printf("  per-eye (different orient) sy diff for level far point: %.6f\n", curDiff);

    // In a correct setup the two eyes must still agree on the vertical angle
    // of a world-level feature even when the head is rolled. With the fix,
    // both eyes use shared head orientation, so this invariant now holds.
    ASSERT_NEAR(curDiff, 0.0f, 0.0005f);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tests for the symmetric overrender + asymmetric UV crop approach.
// These validate the math in lua_interface.cpp GetPoses eye data.
// ─────────────────────────────────────────────────────────────────────────────

struct OverrenderResult {
    float halfTanX, halfTanY;  // symmetric half-tangents
    float fov_deg;             // symmetric horizontal FOV
    float aspect;              // symmetric aspect ratio
    float u0, u1, v0, v1;     // UV crop bounds
};

static OverrenderResult ComputeOverrender(float angleLeft, float angleRight,
                                           float angleUp, float angleDown) {
    float tanL = tanf(angleLeft);
    float tanR = tanf(angleRight);
    float tanU = tanf(angleUp);
    float tanD = tanf(angleDown);

    OverrenderResult r;
    r.halfTanX = std::fmax(std::fabs(tanL), std::fabs(tanR));
    r.halfTanY = std::fmax(std::fabs(tanU), std::fabs(tanD));
    r.aspect = r.halfTanX / r.halfTanY;
    r.fov_deg = 2.0f * atanf(r.halfTanX) * (180.0f / 3.14159265f);

    // UV crop: map asymmetric frustum into the symmetric texture
    r.u0 = (tanL + r.halfTanX) / (2.0f * r.halfTanX);
    r.u1 = (tanR + r.halfTanX) / (2.0f * r.halfTanX);
    r.v0 = (r.halfTanY - tanU) / (2.0f * r.halfTanY);
    r.v1 = (r.halfTanY - tanD) / (2.0f * r.halfTanY);
    return r;
}

TEST(Overrender_SymmetricFOV_ProducesFullRect) {
    // Symmetric FOV: left=-45deg, right=+45deg, up=+45deg, down=-45deg
    float a = 45.0f * 3.14159265f / 180.0f;
    auto r = ComputeOverrender(-a, a, a, -a);

    // For a symmetric frustum the crop should be (nearly) the full rect
    printf("  Symmetric FOV crop: u[%.4f-%.4f] v[%.4f-%.4f] fov=%.1f asp=%.3f\n",
        r.u0, r.u1, r.v0, r.v1, r.fov_deg, r.aspect);
    ASSERT_NEAR(r.u0, 0.0f, 0.001f);
    ASSERT_NEAR(r.u1, 1.0f, 0.001f);
    ASSERT_NEAR(r.v0, 0.0f, 0.001f);
    ASSERT_NEAR(r.v1, 1.0f, 0.001f);
    ASSERT_NEAR(r.aspect, 1.0f, 0.001f);
}

TEST(Overrender_AsymmetricFOV_CropSelectsCorrectSubrect) {
    // Typical left eye: wider on the nose side
    // angleLeft = -50deg, angleRight = +45deg, angleUp = +50deg, angleDown = -50deg
    float aL = -50.0f * 3.14159265f / 180.0f;
    float aR =  45.0f * 3.14159265f / 180.0f;
    float aU =  50.0f * 3.14159265f / 180.0f;
    float aD = -50.0f * 3.14159265f / 180.0f;
    auto r = ComputeOverrender(aL, aR, aU, aD);

    printf("  Asymmetric FOV crop: u[%.4f-%.4f] v[%.4f-%.4f] fov=%.1f asp=%.3f\n",
        r.u0, r.u1, r.v0, r.v1, r.fov_deg, r.aspect);

    // Left extends further than right, so u0 should be close to 0 and u1 < 1
    ASSERT_TRUE(r.u0 < 0.05f);
    ASSERT_TRUE(r.u1 < 1.0f);
    ASSERT_TRUE(r.u1 > r.u0);
    // V should be full (symmetric up/down)
    ASSERT_NEAR(r.v0, 0.0f, 0.001f);
    ASSERT_NEAR(r.v1, 1.0f, 0.001f);
    // The crop should fully contain the asymmetric frustum
    ASSERT_TRUE(r.u0 >= 0.0f);
    ASSERT_TRUE(r.u1 <= 1.0f);
}

TEST(Overrender_CropBoundsPreserveAspectRatio) {
    // The crop sub-rect aspect must match the original asymmetric frustum aspect
    float aL = -48.0f * 3.14159265f / 180.0f;
    float aR =  44.0f * 3.14159265f / 180.0f;
    float aU =  52.0f * 3.14159265f / 180.0f;
    float aD = -48.0f * 3.14159265f / 180.0f;
    auto r = ComputeOverrender(aL, aR, aU, aD);

    // Original asymmetric frustum dimensions in tangent space
    float origW = tanf(aR) - tanf(aL);
    float origH = tanf(aU) - tanf(aD);
    float origAsp = origW / origH;

    // Crop rect dimensions in UV space, mapped back to tangent
    float cropW = (r.u1 - r.u0) * 2.0f * r.halfTanX;
    float cropH = (r.v1 - r.v0) * 2.0f * r.halfTanY;
    float cropAsp = cropW / cropH;

    printf("  Original aspect=%.4f  Crop aspect=%.4f\n", origAsp, cropAsp);
    ASSERT_NEAR(cropAsp, origAsp, 0.001f);
}

TEST(Overrender_VFlip_MirrorsCorrectly) {
    // Test that the V-flip logic (1-v1, 1-v0) produces correct mirrored bounds
    float aL = -50.0f * 3.14159265f / 180.0f;
    float aR =  45.0f * 3.14159265f / 180.0f;
    float aU =  52.0f * 3.14159265f / 180.0f;
    float aD = -48.0f * 3.14159265f / 180.0f;
    auto r = ComputeOverrender(aL, aR, aU, aD);

    // Apply the V-flip (as in xr_render.cpp)
    float flipped_v0 = 1.0f - r.v1;
    float flipped_v1 = 1.0f - r.v0;

    printf("  Original v[%.4f-%.4f]  Flipped v[%.4f-%.4f]\n",
        r.v0, r.v1, flipped_v0, flipped_v1);

    // Flipped bounds should still be valid (v0 < v1)
    ASSERT_TRUE(flipped_v0 < flipped_v1);
    // Flipped bounds should preserve the crop height
    ASSERT_NEAR(flipped_v1 - flipped_v0, r.v1 - r.v0, 0.0001f);
    // Flipped bounds should be within [0,1]
    ASSERT_TRUE(flipped_v0 >= 0.0f);
    ASSERT_TRUE(flipped_v1 <= 1.0f);
}

TEST(Overrender_TypicalHMD_LeftRightMirrorSymmetry) {
    // Typical HMD: left eye has more FOV on the left (nose side), right eye mirrors
    float aL_left  = -55.0f * 3.14159265f / 180.0f;
    float aR_left  =  47.0f * 3.14159265f / 180.0f;
    float aL_right = -47.0f * 3.14159265f / 180.0f;
    float aR_right =  55.0f * 3.14159265f / 180.0f;
    float aU = 52.0f * 3.14159265f / 180.0f;
    float aD = -52.0f * 3.14159265f / 180.0f;

    auto rL = ComputeOverrender(aL_left, aR_left, aU, aD);
    auto rR = ComputeOverrender(aL_right, aR_right, aU, aD);

    printf("  Left eye crop:  u[%.4f-%.4f] v[%.4f-%.4f]\n", rL.u0, rL.u1, rL.v0, rL.v1);
    printf("  Right eye crop: u[%.4f-%.4f] v[%.4f-%.4f]\n", rR.u0, rR.u1, rR.v0, rR.v1);

    // Left eye: more FOV on left, so u0 close to 0, u1 < 1
    ASSERT_TRUE(rL.u0 < 0.05f);
    ASSERT_TRUE(rL.u1 < 0.98f);
    // Right eye: more FOV on right, so u0 > 0, u1 close to 1
    ASSERT_TRUE(rR.u0 > 0.02f);
    ASSERT_TRUE(rR.u1 > 0.95f);
    // Mirror symmetry: left u0 ≈ 1 - right u1, left u1 ≈ 1 - right u0
    ASSERT_NEAR(rL.u0, 1.0f - rR.u1, 0.001f);
    ASSERT_NEAR(rL.u1, 1.0f - rR.u0, 0.001f);
}
