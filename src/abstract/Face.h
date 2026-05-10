#pragma once

#include <cstdint>

namespace abstract {

// Polygon face selector, used for per-face stencil operations.
enum class Face : uint8_t {
  kFront,  // Front-facing polygons.
  kBack,   // Back-facing polygons.
};

}  // namespace abstract
