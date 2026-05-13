#pragma once

#include "core/Color.h"
#include "core/Vec3f.h"

namespace game {

// Descriptor holding parameters for all renderer light types.
//
// Fields unused by the chosen LightType are harmlessly ignored at construction.
// All fields carry sensible defaults so only the relevant ones need to be set.
//
//   GameLightDesc desc;
//   desc.color    = core::Color(1.f, 0.5f, 0.f);
//   desc.radius   = 30.f;
//   GameLight light(renderer::LightType::kOmni, desc);
struct GameLightDesc {
  core::Color  color         = core::Color(1.f, 1.f, 1.f);   // All types
  float        intensity     = 1.f;                           // All types
  float        radius        = 10.f;                          // OmniLight
  core::Vec3f  direction     = core::Vec3f(0.f, -1.f, 0.f);  // GlobalLight + spot lights
  core::Vec3f  ambient_color = core::Vec3f(0.f, 0.f, 0.f);   // GlobalLight
  float        inner_angle   = 0.2f;                          // CircleSpotLight
  float        outer_angle   = 0.4f;                          // CircleSpotLight
  float        range         = 20.f;                          // CircleSpotLight + RectangleSpotLight
  float        h_angle            = 0.4f;    // RectangleSpotLight
  float        v_angle            = 0.3f;    // RectangleSpotLight
  bool         cast_shadow        = true;    // All types: false skips shadow map entirely
  int          shadow_resolution  = 1024;    // Max shadow map resolution cap (pool may assign lower)
  float        shadow_bias        = 0.005f;  // Depth bias to avoid self-shadowing acne
};

}  // namespace game
