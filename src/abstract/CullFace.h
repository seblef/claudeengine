#pragma once

#include <cstdint>

namespace abstract {

// Face culling mode for rasterization.
enum class CullFace : uint8_t {
  kNone,   // No culling — draw both front and back faces.
  kFront,  // Cull front faces.
  kBack,   // Cull back faces (standard default).
};

}  // namespace abstract
