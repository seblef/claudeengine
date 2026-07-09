#pragma once

#include "core/Mat4f.h"

namespace renderer {

// Per-cascade output of GlobalLight::ComputeCascadeBasis(): the light-space
// frame for a cascade, independent of shadow casters. ShadowRenderer::
// RenderCascades unions receiver_min_z/receiver_max_z with the light-space Z
// extents of actual casters (found via a broad-phase probe query) to derive
// the tight, caster-aware near/far planes and the final cascade_vp.
struct CascadeLightBasis {
  core::Mat4f light_view;
  float min_x = 0.f, max_x = 0.f;
  float min_y = 0.f, max_y = 0.f;
  float receiver_min_z = 0.f, receiver_max_z = 0.f;
};

}  // namespace renderer
