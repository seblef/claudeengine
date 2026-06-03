#pragma once

#include <cstddef>

namespace environment {

// Per-frame constant buffer for the water shader (slot 9).
// std140 layout — must match data/shaders/glsl/uniforms/water_infos.glsl exactly.
//
// std140 offsets (all members are float/vec4 — no vec3 alignment issues):
//   [  0] water_color_r         float
//   [  4] water_color_g         float
//   [  8] water_color_b         float
//   [ 12] water_level           float  world-space Y of the undisplaced surface
//   [ 16] sky_zenith_r          float
//   [ 20] sky_zenith_g          float
//   [ 24] sky_zenith_b          float
//   [ 28] roughness             float
//   [ 32] sun_dir_x             float
//   [ 36] sun_dir_y             float
//   [ 40] sun_dir_z             float
//   [ 44] sun_intensity         float
//   [ 48] refraction_strength   float
//   [ 52] absorption_scale      float
//   [ 56] foam_height_thresh    float
//   [ 60] foam_shoreline_depth  float
//   [ 64] foam_steepness_thresh float
//   [ 68] foam_speed            float
//   [ 72] normal_scale1         float
//   [ 76] normal_scale2         float
//   [ 80] normal_scroll_speed1  float
//   [ 84] normal_scroll_speed2  float
//   [ 88] lod_near_dist         float  full quality below this XZ distance (metres)
//   [ 92] lod_far_dist          float  minimal quality above this XZ distance (metres)
// Total: 96 bytes (6 float4s).
struct WaterInfos {
    // float4 0
    // cppcheck-suppress unusedStructMember
    float water_color_r = 0.02f;  // deep water colour, red
    // cppcheck-suppress unusedStructMember
    float water_color_g = 0.08f;  // deep water colour, green
    // cppcheck-suppress unusedStructMember
    float water_color_b = 0.15f;  // deep water colour, blue
    // cppcheck-suppress unusedStructMember
    float water_level   = 0.f;    // world-space Y of the undisplaced surface

    // float4 1
    // cppcheck-suppress unusedStructMember
    float sky_zenith_r  = 0.40f;  // sky zenith colour, red
    // cppcheck-suppress unusedStructMember
    float sky_zenith_g  = 0.65f;  // sky zenith colour, green
    // cppcheck-suppress unusedStructMember
    float sky_zenith_b  = 0.90f;  // sky zenith colour, blue
    // cppcheck-suppress unusedStructMember
    float roughness     = 0.05f;  // surface micro-roughness for specular lobe

    // float4 2
    // cppcheck-suppress unusedStructMember
    float sun_dir_x       = 0.f;   // world-space sun direction, X
    // cppcheck-suppress unusedStructMember
    float sun_dir_y       = 1.f;   // world-space sun direction, Y
    // cppcheck-suppress unusedStructMember
    float sun_dir_z       = 0.f;   // world-space sun direction, Z
    // cppcheck-suppress unusedStructMember
    float sun_intensity   = 20.f;  // HDR sun power multiplier

    // float4 3
    // cppcheck-suppress unusedStructMember
    float refraction_strength  = 0.03f;  // UV distortion scale for refraction
    // cppcheck-suppress unusedStructMember
    float absorption_scale     = 0.20f;  // depth-based colour absorption strength
    // cppcheck-suppress unusedStructMember
    float foam_height_thresh   = 0.60f;  // wave-steepness foam threshold
    // cppcheck-suppress unusedStructMember
    float foam_shoreline_depth = 2.0f;   // world-units depth for shoreline foam

    // float4 4
    // cppcheck-suppress unusedStructMember
    float foam_steepness_thresh   = 0.70f;  // normal-Y steepness threshold for foam
    // cppcheck-suppress unusedStructMember
    float foam_speed              = 1.5f;   // foam texture animation speed
    // cppcheck-suppress unusedStructMember
    float normal_scale1           = 0.03f;  // scale for first normal map layer
    // cppcheck-suppress unusedStructMember
    float normal_scale2           = 0.07f;  // scale for second normal map layer

    // float4 5
    // cppcheck-suppress unusedStructMember
    float normal_scroll_speed1 = 0.30f;  // scroll speed for first normal layer
    // cppcheck-suppress unusedStructMember
    float normal_scroll_speed2 = 0.20f;  // scroll speed for second normal layer
    // cppcheck-suppress unusedStructMember
    float lod_near_dist        = 50.f;   // full quality below this XZ distance (metres)
    // cppcheck-suppress unusedStructMember
    float lod_far_dist         = 100.f;  // minimal quality above this XZ distance (metres)
};

static_assert(sizeof(WaterInfos) == 96);

}  // namespace environment
