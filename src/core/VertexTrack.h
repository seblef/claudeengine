#pragma once

#include "core/Vec2f.h"
#include "core/Vec3f.h"

namespace core {

// Vertex format used by TireTrackSystem for ground-decal quads.
//
// Memory layout (stride = 32 bytes):
//   offset  0 — position  (vec3, 12 bytes)
//   offset 12 — uv        (vec2,  8 bytes)
//   offset 20 — alpha     (float, 4 bytes)  per-vertex fade factor [0, 1]
class VertexTrack {
 public:
    VertexTrack() = default;

    VertexTrack(const Vec3f& position_, const Vec2f& uv_, float alpha_)
        : position(position_), uv(uv_), alpha(alpha_) {}

    Vec3f position;
    Vec2f uv;
    float alpha  = 0.f;
};

}  // namespace core
