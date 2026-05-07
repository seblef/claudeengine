#pragma once

#include "core/Mat4f.h"

namespace renderer {

// GPU constant buffer layout for per-renderable object data (UBO slot 1).
// Matches layout(std140, row_major) exactly.
//
// std140 offsets:
//   [  0] world  mat4  (64 B — 4 float4s)
struct RenderableInfos {
  core::Mat4f world;
};

static_assert(sizeof(RenderableInfos) == 64);

}  // namespace renderer
