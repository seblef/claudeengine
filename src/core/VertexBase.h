#pragma once

#include "core/Color.h"
#include "core/Vec2f.h"
#include "core/Vec3f.h"

namespace core {

// Base vertex: position in 3D space, RGBA color, and UV texture coordinates.
//
// Members are plain floats with no padding or alignment hints so the struct
// maps directly to a GPU vertex buffer without surprises.
class VertexBase {
 public:
  VertexBase() = default;
  VertexBase(const VertexBase&) = default;
  VertexBase& operator=(const VertexBase&) = default;

  VertexBase(const Vec3f& position, const Color& color, const Vec2f& uv)
      : position(position), color(color), uv(uv) {}

  Vec3f position;
  Color color;
  Vec2f uv;
};

}  // namespace core
