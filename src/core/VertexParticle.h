#pragma once

#include "core/Color.h"
#include "core/Vec2f.h"
#include "core/Vec3f.h"

namespace core {

// Billboard vertex for the particle renderer.
//
// All four vertices of a billboard quad carry the same center, size, color,
// uv_offset and rotation. The vertex shader expands the center into
// screen-space corners using gl_VertexID % 4 to determine the corner index.
//
// uv_offset2 / frame_blend support smooth inter-frame interpolation: the shader
// samples both sprite-sheet frames and blends between them (mix(frame0, frame1,
// frame_blend)). Set frame_blend = 0 and uv_offset2 = uv_offset to disable.
//
// Memory layout (stride = sizeof(VertexParticle)):
//   offset  0 — center      (vec3,  12 bytes)
//   offset 12 — size        (float,  4 bytes)
//   offset 16 — color       (vec4,  16 bytes, alignas(16))
//   offset 32 — uv_offset   (vec2,   8 bytes)
//   offset 40 — rotation    (float,  4 bytes)
//   offset 44 — uv_offset2  (vec2,   8 bytes)  // next-frame sprite UV
//   offset 52 — frame_blend (float,  4 bytes)  // [0,1] blend toward next frame
//   total 56 bytes (padded to 64 by Color's alignas(16))
class VertexParticle {
 public:
  VertexParticle() = default;
  VertexParticle(const VertexParticle&) = default;
  VertexParticle& operator=(const VertexParticle&) = default;

  VertexParticle(const Vec3f& center, float size,
                 const Color& color, const Vec2f& uv_offset, float rotation = 0.f)
      : center(center), size(size), color(color), uv_offset(uv_offset),
        rotation(rotation), uv_offset2(uv_offset), frame_blend(0.f) {}

  Vec3f center;
  float size        = 0.f;
  Color color;
  Vec2f uv_offset;
  float rotation    = 0.f;
  Vec2f uv_offset2;    // next-frame sprite UV for inter-frame interpolation
  float frame_blend = 0.f;  // blend factor [0,1] between uv_offset and uv_offset2
};

}  // namespace core
