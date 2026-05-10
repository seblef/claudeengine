#pragma once

#include <cstddef>

#include "core/Vec3f.h"

namespace renderer {

// GPU constant buffer layout for per-material color data (UBO slot 3).
// Matches layout(std140, binding=3) exactly — do not reorder fields.
//
// std140 offsets (Vec3f is alignas(16), w_=0 fills the implicit vec3 pad):
//   [  0] diffuse_color   vec3  (16 B — 3 floats + w_=0 as std140 padding)
//   [ 16] emissive_color  vec3  (16 B)
//   [ 32] ambient_color   vec3  (16 B)
//   [ 48] shininess       float ( 4 B)
//   [ 52] end / tail pad  12 B  (struct padded to alignof = 16)
struct MaterialInfos {
  core::Vec3f diffuse_color;   // diffuse texture tint; defaults to (1,1,1)
  core::Vec3f emissive_color;  // emissive texture scale; defaults to (0,0,0)
  core::Vec3f ambient_color;   // additive ambient term; defaults to (0,0,0)
  float       shininess;       // Blinn-Phong exponent; defaults to 32
};

static_assert(sizeof(MaterialInfos) == 64);
static_assert(offsetof(MaterialInfos, emissive_color) == 16);
static_assert(offsetof(MaterialInfos, ambient_color)  == 32);
static_assert(offsetof(MaterialInfos, shininess)      == 48);

}  // namespace renderer
