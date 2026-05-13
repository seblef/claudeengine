#pragma once

#include <cstddef>

#include "core/Mat4f.h"

namespace renderer {

constexpr int kCSMCascadeCount = 4;

// GPU constant buffer layout for Cascaded Shadow Map data (UBO slot 5).
// Matches layout(std140, row_major, binding=5) exactly — do not reorder fields.
//
// Offset map:
//   [  0] cascade_vp[0..3]  4 × mat4  (4 × 64 B = 256 B — light-space VP per cascade)
//   [256] split_x,y,z,w     vec4      ( 16 B — view-space far depths for cascades 0–3)
//   [272] end
struct CSMInfos {
  core::Mat4f cascade_vp[kCSMCascadeCount];  // [  0..255]
  float split_x, split_y, split_z, split_w;  // [256..271]
};

static_assert(sizeof(CSMInfos) == 272);
static_assert(offsetof(CSMInfos, split_x) == 256);

}  // namespace renderer
