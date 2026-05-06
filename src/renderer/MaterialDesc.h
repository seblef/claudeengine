#pragma once

#include <array>
#include <string>

#include "renderer/TextureSlot.h"

namespace renderer {

// Descriptor for constructing a Material from texture names.
// Only the slots you care about need to be set; unset slots (empty string)
// fall back to the material's default textures.
//
// Typical usage:
//   Material mat(MaterialDesc().SetDiffuse("hero.png").SetNormal("hero_n.png"), video);
class MaterialDesc {
 public:
  // Sets the texture name for the given slot. Returns *this for chaining.
  MaterialDesc& Set(TextureSlot slot, std::string name) {
    names_[static_cast<int>(slot)] = std::move(name);
    return *this;
  }

  MaterialDesc& SetDiffuse(std::string name) {
    return Set(TextureSlot::kDiffuse, std::move(name));
  }
  MaterialDesc& SetNormal(std::string name) {
    return Set(TextureSlot::kNormal, std::move(name));
  }
  MaterialDesc& SetSpecular(std::string name) {
    return Set(TextureSlot::kSpecular, std::move(name));
  }
  MaterialDesc& SetEmissive(std::string name) {
    return Set(TextureSlot::kEmissive, std::move(name));
  }
  MaterialDesc& SetEnvironment(std::string name) {
    return Set(TextureSlot::kEnvironment, std::move(name));
  }

  // Returns the texture name for slot (empty string means "use default").
  [[nodiscard]] const std::string& Get(TextureSlot slot) const {
    return names_[static_cast<int>(slot)];
  }

 private:
  std::array<std::string, kTextureSlotCount> names_{};
};

}  // namespace renderer
