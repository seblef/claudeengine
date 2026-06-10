#pragma once

#include "core/Color.h"
#include "core/Vec2f.h"
#include "core/Vec3f.h"

namespace core {

// Billboard vertex for the particle renderer.
//
// All four vertices of a billboard quad carry the same center, size, color and
// uv_offset. The vertex shader expands the center into screen-space corners
// using gl_VertexID % 4 to determine the corner index.
//
// Memory layout (stride = sizeof(VertexParticle)):
//   offset  0 — center    (vec3,  12 bytes)
//   offset 12 — size      (float,  4 bytes)
//   offset 16 — color     (vec4,  16 bytes, alignas(16))
//   offset 32 — uv_offset (vec2,   8 bytes)
//   total 48 bytes (padded to multiple of 16 by Color's alignas(16))
class VertexParticle {
 public:
  VertexParticle() = default;
  VertexParticle(const VertexParticle&) = default;
  VertexParticle& operator=(const VertexParticle&) = default;

  VertexParticle(const Vec3f& center, float size,
                 const Color& color, const Vec2f& uv_offset)
      : center(center), size(size), color(color), uv_offset(uv_offset) {}

  Vec3f center;
  float size      = 0.f;
  Color color;
  Vec2f uv_offset;
};

}  // namespace core
