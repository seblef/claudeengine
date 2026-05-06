#pragma once

namespace renderer {

// Number of texture slots in a material.
constexpr int kTextureSlotCount = 5;

// Normalized texture slot indices.
// Each slot has a fixed binding point used by Set() to bind textures to
// the corresponding texture units.
enum class TextureSlot : int {
  kDiffuse     = 0,
  kNormal      = 1,
  kSpecular    = 2,
  kEmissive    = 3,
  kEnvironment = 4,
};

}  // namespace renderer
