#pragma once

#include <cstdint>

namespace abstract {

// Topology used for subsequent draw calls.
enum class PrimitiveType : uint8_t {
  kPointList,      // Each vertex is an independent point
  kLineList,       // Each pair of vertices forms an independent line
  kLineStrip,      // Vertices form a connected strip of lines
  kTriangleList,   // Each triplet of vertices forms an independent triangle
  kTriangleStrip,  // Vertices form a connected strip of triangles
  kTriangleFan,    // Vertices fan around the first vertex
  kPatch4,         // 4-vertex quad patch for hardware tessellation (GL_PATCHES)
};

}  // namespace abstract
