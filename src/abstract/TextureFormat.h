#pragma once

#include <cstdint>

namespace abstract {

// GPU texture internal formats.
enum class TextureFormat : uint8_t {
  kA8R8G8B8,
  kX8R8G8B8,
  kR16F,
  kR32F,
  kR32G32F,
  kG16R16F,
  kA16R16G16B16F,
  kA32R32G32B32F,
  kG11R11B10F,
};

}  // namespace abstract
