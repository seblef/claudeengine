#pragma once

#include <cstdint>

namespace core {

// Camera projection type.
enum class ProjectionType : uint8_t {
  kPerspective,
  kOrthographic,
};

}  // namespace core
