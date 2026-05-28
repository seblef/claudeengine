#pragma once

#include <cstddef>

namespace environment {

// Per-frame constant buffer for the water shader (slot 9).
// std140 layout — must match data/shaders/glsl/uniforms/water_infos.glsl exactly.
//
// std140 offsets (all members are float/vec4 — no vec3 alignment issues):
//   [  0] water_color_r   float
//   [  4] water_color_g   float
//   [  8] water_color_b   float
//   [ 12] water_level     float   world-space Y of the undisplaced surface
//   [ 16] sky_zenith_r    float
//   [ 20] sky_zenith_g    float
//   [ 24] sky_zenith_b    float
//   [ 28] wi_pad0_        float
//   [ 32] sun_dir_x       float
//   [ 36] sun_dir_y       float
//   [ 40] sun_dir_z       float
//   [ 44] wi_pad1_        float
// Total: 48 bytes (3 float4s).
struct WaterInfos {
    // cppcheck-suppress unusedStructMember
    float water_color_r = 0.02f;  // deep water colour, red
    // cppcheck-suppress unusedStructMember
    float water_color_g = 0.08f;  // deep water colour, green
    // cppcheck-suppress unusedStructMember
    float water_color_b = 0.15f;  // deep water colour, blue
    // cppcheck-suppress unusedStructMember
    float water_level   = 0.f;    // world-space Y of the undisplaced surface

    // cppcheck-suppress unusedStructMember
    float sky_zenith_r  = 0.40f;  // sky zenith colour, red
    // cppcheck-suppress unusedStructMember
    float sky_zenith_g  = 0.65f;  // sky zenith colour, green
    // cppcheck-suppress unusedStructMember
    float sky_zenith_b  = 0.90f;  // sky zenith colour, blue
    // cppcheck-suppress unusedStructMember
    float wi_pad0_      = 0.f;

    // cppcheck-suppress unusedStructMember
    float sun_dir_x     = 0.f;    // world-space sun direction, X
    // cppcheck-suppress unusedStructMember
    float sun_dir_y     = 1.f;    // world-space sun direction, Y
    // cppcheck-suppress unusedStructMember
    float sun_dir_z     = 0.f;    // world-space sun direction, Z
    // cppcheck-suppress unusedStructMember
    float wi_pad1_      = 0.f;
};

static_assert(sizeof(WaterInfos) == 48);

}  // namespace environment
