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
  // Render-target formats (not used for file-loaded textures).
  kRGBA8,           // 4×8-bit unsigned normalised — G-buffer albedo/specular.
  kRGBA16F,         // 4×16-bit half-float — G-buffer normals and HDR accumulation.
  kDepth24Stencil8,  // 24-bit depth + 8-bit stencil — G-buffer depth+stencil.
};

}  // namespace abstract
