#pragma once

#include <cstddef>

#include "core/Mat4f.h"
#include "core/Vec2f.h"
#include "core/Vec3f.h"

namespace renderer {

// GPU constant buffer layout for per-frame scene data (UBO slot 2).
// Matches layout(std140, row_major) exactly — do not reorder fields.
//
// std140 offsets (vec3 consumes 12 bytes; no implicit padding before next float):
//   [  0] view_proj        mat4  (64 B)
//   [ 64] inv_view_proj    mat4  (64 B)
//   [128] inv_proj         mat4  (64 B)
//   [192] proj             mat4  (64 B)
//   [256] view             mat4  (64 B)
//   [320] eye_pos          vec3  (12 B — Vec3f = 3 floats, no w_ padding)
//   [332] time             float ( 4 B)
//   [336] inv_screen_size  vec2  ( 8 B)
//   [344] z_near_          float ( 4 B) — camera near plane (for depth linearization)
//   [348] z_far_           float ( 4 B) — camera far plane  (for depth linearization)
//   [352] end
struct SceneInfos {
  core::Mat4f view_proj;
  core::Mat4f inv_view_proj;
  core::Mat4f inv_proj;
  core::Mat4f proj;
  core::Mat4f view;
  core::Vec3f eye_pos;          // world-space camera position
  float       time;             // elapsed time in seconds
  core::Vec2f inv_screen_size;  // (1/width, 1/height)
  float       z_near_ = 0.f;   // camera near plane distance
  float       z_far_  = 1.f;   // camera far plane distance
};

static_assert(sizeof(SceneInfos) == 352);
static_assert(offsetof(SceneInfos, eye_pos)         == 320);
static_assert(offsetof(SceneInfos, time)            == 332);
static_assert(offsetof(SceneInfos, inv_screen_size) == 336);
static_assert(offsetof(SceneInfos, z_near_)         == 344);
static_assert(offsetof(SceneInfos, z_far_)          == 348);

}  // namespace renderer
