#pragma once

#include "core/Vec2f.h"

namespace environment {

// Per-frame constant buffer for wind data (slot 7).
// std140 layout — must match data/shaders/glsl/uniforms/wind_infos.glsl exactly.
//
// std140 offsets:
//   [  0] wind_xz       vec2  (8 bytes; 8-byte alignment)
//   [  8] wind_strength float (4 bytes)
//   [ 12] wind_time     float (4 bytes)
// Total: 16 bytes (1 float4).
struct WindInfos {
    // cppcheck-suppress unusedStructMember
    core::Vec2f wind_xz;           // wind direction × speed, XZ plane (m/s)
    // cppcheck-suppress unusedStructMember
    float       wind_strength = 0.f;  // scalar speed magnitude (m/s)
    // cppcheck-suppress unusedStructMember
    float       wind_time     = 0.f;  // accumulated simulation time (s)
};

static_assert(sizeof(WindInfos) == 16);

}  // namespace environment
