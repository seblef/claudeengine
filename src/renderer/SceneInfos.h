#pragma once

#include <cstddef>

#include "core/Mat4f.h"
#include "core/Vec2f.h"
#include "core/Vec3f.h"

namespace renderer {

// GPU constant buffer layout for per-frame scene data (UBO slot 1).
// Matches layout(std140, row_major) exactly — do not reorder fields.
//
// std140 offsets:
//   [  0] view_proj        mat4  (64 B)
//   [ 64] inv_view_proj    mat4  (64 B)
//   [128] inv_proj         mat4  (64 B)
//   [192] proj             mat4  (64 B)
//   [256] view             mat4  (64 B)
//   [320] eye_pos          vec3  (16 B — Vec3f is alignas(16) with w_=0 padding)
//   [336] time             float ( 4 B)
//   [340] pad0_            float ( 4 B — explicit gap; vec2 needs align-8 at 344)
//   [344] inv_screen_size  vec2  ( 8 B)
//   [352] end              22 float4s total
struct SceneInfos {
  core::Mat4f view_proj;
  core::Mat4f inv_view_proj;
  core::Mat4f inv_proj;
  core::Mat4f proj;
  core::Mat4f view;
  core::Vec3f eye_pos;          // world-space camera position
  float       time;             // elapsed time in seconds
  float       pad0_ = 0.f;     // std140 alignment gap before inv_screen_size
  core::Vec2f inv_screen_size;  // (1/width, 1/height)
};

static_assert(sizeof(SceneInfos) == 352);
static_assert(offsetof(SceneInfos, eye_pos)         == 320);
static_assert(offsetof(SceneInfos, time)            == 336);
static_assert(offsetof(SceneInfos, pad0_)           == 340);
static_assert(offsetof(SceneInfos, inv_screen_size) == 344);

}  // namespace renderer
