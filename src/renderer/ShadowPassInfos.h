#pragma once

#include <cstddef>

#include "core/Mat4f.h"
#include "core/Vec4f.h"

namespace renderer {

// Constant buffer layout for the shadow depth pass (slot 6, 80 bytes, 5 float4s).
// Uploaded once per light before rendering shadow casters.
//
// light_pos_range is only used by the cube shadow depth shader; leave zeroed
// for CSM and spot-light passes.
struct ShadowPassInfos {
  core::Mat4f light_vp;
  core::Vec4f light_pos_range;  // xyz = world position, w = range
};

static_assert(sizeof(ShadowPassInfos) == 80,
              "ShadowPassInfos must be exactly 80 bytes (5 float4s)");

}  // namespace renderer
