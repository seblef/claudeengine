#pragma once

#include "core/Vec2f.h"

namespace environment {

// Per-frame constant buffer for wind data (slot 7).
// std140 layout — must match data/shaders/glsl/uniforms/wind_infos.glsl exactly.
//
// std140 offsets:
//   [  0] wind_xz          vec2  (8 bytes; 8-byte alignment)
//   [  8] wind_strength     float (4 bytes)
//   [ 12] wind_time         float (4 bytes)
//   [ 16] wind_displacement vec2  (8 bytes; 8-byte alignment)
//   [ 24] _pad              vec2  (8 bytes padding to reach 32 = 2 float4s)
// Total: 32 bytes (2 float4s).
struct WindInfos {
    // cppcheck-suppress unusedStructMember
    core::Vec2f wind_xz;                      // wind direction × speed, XZ plane (m/s)
    // cppcheck-suppress unusedStructMember
    float       wind_strength    = 0.f;       // scalar speed magnitude (m/s)
    // cppcheck-suppress unusedStructMember
    float       wind_time        = 0.f;       // accumulated simulation time (s)
    // cppcheck-suppress unusedStructMember
    core::Vec2f wind_displacement;            // accumulated XZ displacement (m)
    // cppcheck-suppress unusedStructMember
    float       pad0_            = 0.f;
    // cppcheck-suppress unusedStructMember
    float       pad1_            = 0.f;
};

static_assert(sizeof(WindInfos) == 32);

}  // namespace environment
