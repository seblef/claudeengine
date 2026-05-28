#pragma once

#include "core/Vec3f.h"

namespace environment {

// Per-frame constant buffer for the sky shader (slot 8).
// std140 layout — must match data/shaders/glsl/uniforms/sky_infos.glsl exactly.
//
// std140 offsets:
//   [  0] sun_direction  vec3  (12 bytes; std140 aligns vec3 to 16 → 4-byte pad follows)
//   [ 12] si_pad0_       float
//   [ 16] time_of_day    float (hours, 0–24)
//   [ 20] turbidity      float (1.7 = very clear, 10 = very hazy)
//   [ 24] si_pad1_       float
//   [ 28] si_pad2_       float
// Total: 32 bytes (2 float4s).
struct SkyInfos {
    // cppcheck-suppress unusedStructMember
    core::Vec3f sun_direction;         // world-space unit vector toward sun
    // cppcheck-suppress unusedStructMember
    float       si_pad0_  = 0.f;
    // cppcheck-suppress unusedStructMember
    float       time_of_day = 12.f;   // 0–24 hours
    // cppcheck-suppress unusedStructMember
    float       turbidity   = 2.f;    // atmospheric haziness
    // cppcheck-suppress unusedStructMember
    float       si_pad1_  = 0.f;
    // cppcheck-suppress unusedStructMember
    float       si_pad2_  = 0.f;
};

}  // namespace environment
