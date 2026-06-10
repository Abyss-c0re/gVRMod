#pragma once

#include "core/vrmod_common.h"
#include <cstring>
#include <vector>
#include <string>

namespace mock {

// ── PoseResult test helper ──
// Build a PoseResult with known values for testing (runtime-agnostic)
inline PoseResult MakePoseResult(
    float px, float py, float pz,       // position (Source engine coords)
    float vx, float vy, float vz,       // velocity (Source engine coords)
    float ax, float ay, float az,       // angles (degrees, Source engine)
    float avx, float avy, float avz,    // angular velocity (degrees/s, Source engine)
    bool valid = true)
{
    PoseResult r;
    memset(&r, 0, sizeof(r));
    r.valid = valid;
    if (valid) {
        r.pos[0] = px; r.pos[1] = py; r.pos[2] = pz;
        r.vel[0] = vx; r.vel[1] = vy; r.vel[2] = vz;
        r.ang[0] = ax; r.ang[1] = ay; r.ang[2] = az;
        r.angvel[0] = avx; r.angvel[1] = avy; r.angvel[2] = avz;
    }
    return r;
}

} // namespace mock
