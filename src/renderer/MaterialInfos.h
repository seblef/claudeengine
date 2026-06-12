#pragma once

#include <cstddef>

#include "core/Color.h"

namespace renderer {

// GPU constant buffer layout for per-material color data (UBO slot 3).
// Matches layout(std140, binding=3) exactly — do not reorder fields.
//
// Color is alignas(16) with 4 floats (r,g,b,a) = 16 bytes; maps to vec4 in GLSL.
// std140 offsets:
//   [  0] diffuse_color   vec4  (16 B — RGBA diffuse tint)
//   [ 16] emissive_color  vec4  (16 B — RGBA emissive scale)
//   [ 32] ambient_color   vec4  (16 B — RGBA ambient term)
//   [ 48] shininess       float ( 4 B — Blinn-Phong exponent)
//   [ 52] specular        float ( 4 B — specular intensity multiplier)
//   [ 56] end / tail pad   8 B  (struct padded to alignof(Color) = 16)
struct MaterialInfos {
  core::Color diffuse_color;   // diffuse texture tint; defaults to white opaque
  core::Color emissive_color;  // emissive texture scale; defaults to transparent
  core::Color ambient_color;   // additive ambient term; defaults to transparent
  float       shininess;       // Blinn-Phong exponent; defaults to 32
  float       specular;        // multiplier applied to specular texture sample; defaults to 1
};

static_assert(sizeof(MaterialInfos) == 64);
static_assert(offsetof(MaterialInfos, emissive_color) == 16);
static_assert(offsetof(MaterialInfos, ambient_color)  == 32);
static_assert(offsetof(MaterialInfos, shininess)      == 48);
static_assert(offsetof(MaterialInfos, specular)       == 52);

}  // namespace renderer
