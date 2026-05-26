#pragma once

#include "core/Vec2f.h"

namespace core {

// Terrain vertex — XZ position only (two floats).
//
// Y is omitted because the vertex shader samples the heightmap at runtime and
// derives the world Y from the patch origin + heightmap lookup. UVs are also
// omitted; the VS derives them from the XZ position and the patch scale uniform.
class VertexTerrain {
 public:
  VertexTerrain() = default;
  VertexTerrain(const VertexTerrain&) = default;
  VertexTerrain& operator=(const VertexTerrain&) = default;

  explicit VertexTerrain(const Vec2f& xz) : xz(xz) {}
  VertexTerrain(float x, float z) : xz(x, z) {}

  Vec2f xz;
};

}  // namespace core
