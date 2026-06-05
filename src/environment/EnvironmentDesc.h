#pragma once

#include "core/Vec3f.h"

namespace environment {

// Descriptor for all environment subsystems loaded from a map's YAML section.
// Plain data — no behaviour. Owned by the map loader and consumed by
// individual subsystem classes (WorldTime, sky, water, wind, etc.).
struct EnvironmentDesc {
    bool sky_enabled   = false;
    bool water_enabled = false;
    bool cloud_enabled = false;
    bool wind_enabled  = false;
    bool trees_enabled = false;

    // World-space Y of the water surface.
    float water_level = 0.f;
    // Deep water colour (linear RGB).
    float water_color_r = 0.02f;
    float water_color_g = 0.08f;
    float water_color_b = 0.15f;
    // Surface micro-roughness for specular lobe.
    float roughness = 0.05f;
    // HDR sun power multiplier.
    float sun_intensity = 20.f;
    // UV distortion scale for refraction.
    float refraction_strength = 0.03f;
    // Depth-based colour absorption strength.
    float absorption_scale = 0.20f;
    // Foam parameters.
    float foam_height_thresh   = 0.60f;
    float foam_shoreline_depth = 2.0f;
    float foam_steepness_thresh = 0.70f;
    float foam_speed = 1.5f;
    // Normal map scale and scroll speed for two layers.
    float normal_scale1        = 0.03f;
    float normal_scale2        = 0.07f;
    float normal_scroll_speed1 = 0.30f;
    float normal_scroll_speed2 = 0.20f;
    // Starting time of day in hours [0, 24). Defaults to noon.
    float start_time_of_day = 12.f;
    // Time multiplier: 1.0 = real time, 60.0 = 1 min real → 1 hr in-game.
    float time_scale = 1.f;
    // Cloud coverage in [0, 1].
    float cloud_density = 0.3f;
    // Wind speed in m/s.
    float wind_strength = 3.f;
    // World-space XZ unit vector (Y component is ignored at runtime).
    core::Vec3f wind_direction = {1.f, 0.f, 0.f};
    // Atmospheric turbidity: 1.7 = very clear, 2.0 = default, 10.0 = very hazy.
    float turbidity = 2.f;
};

}  // namespace environment
