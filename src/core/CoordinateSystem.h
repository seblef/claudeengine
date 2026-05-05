#pragma once

#include <cstdint>

namespace core {

// 3D coordinate system handedness.
enum class CoordinateSystem : uint8_t {
  kLeftHanded,   // DirectX convention, camera looks down +Z
  kRightHanded,  // OpenGL convention, camera looks down -Z
};

}  // namespace core
