#pragma once

#include "core/Vec2f.h"
#include "core/Vec3f.h"

namespace core {

// 3D vertex for lit geometry: position, tangent-space basis (normal, binormal,
// tangent), and UV texture coordinates. No color — lighting is evaluated at
// runtime in shaders.
class Vertex3D {
 public:
  Vertex3D() = default;
  Vertex3D(const Vertex3D&) = default;
  Vertex3D& operator=(const Vertex3D&) = default;

  Vertex3D(const Vec3f& position, const Vec3f& normal, const Vec3f& binormal,
           const Vec3f& tangent, const Vec2f& uv)
      : position(position),
        normal(normal),
        binormal(binormal),
        tangent(tangent),
        uv(uv) {}

  Vec3f position;
  Vec3f normal;
  Vec3f binormal;
  Vec3f tangent;
  Vec2f uv;
};

}  // namespace core
