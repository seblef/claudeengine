#pragma once

#include <cstddef>

#include "core/Mat4f.h"

namespace renderer {

// Constant buffer layout for the shadow depth pass (slot 6, 64 bytes, 4 float4s).
// Uploaded once per light before rendering shadow casters.
struct ShadowPassInfos {
  core::Mat4f light_vp;
};

static_assert(sizeof(ShadowPassInfos) == 64,
              "ShadowPassInfos must be exactly 64 bytes (4 float4s)");

}  // namespace renderer
