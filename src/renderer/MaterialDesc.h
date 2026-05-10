#pragma once

#include <array>
#include <string>

#include "core/Color.h"
#include "renderer/TextureSlot.h"

namespace renderer {

// Descriptor for constructing a Material from texture names and color properties.
// Only the fields you care about need to be set; unset fields fall back to the
// material's defaults (white diffuse, no emissive/ambient, shininess=32).
//
// Typical usage:
//   Material mat(MaterialDesc()
//       .SetDiffuse("hero.png")
//       .SetNormal("hero_n.png")
//       .SetDiffuseColor({1.f, 0.8f, 0.6f}), video);
class MaterialDesc {
 public:
  // ---- Texture slots -------------------------------------------------------

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

  // ---- Color properties ----------------------------------------------------

  MaterialDesc& SetDiffuseColor(core::Color color) {
    diffuse_color_ = color;
    return *this;
  }
  MaterialDesc& SetEmissiveColor(core::Color color) {
    emissive_color_ = color;
    return *this;
  }
  MaterialDesc& SetAmbientColor(core::Color color) {
    ambient_color_ = color;
    return *this;
  }
  MaterialDesc& SetShininess(float shininess) {
    shininess_ = shininess;
    return *this;
  }

  [[nodiscard]] core::Color GetDiffuseColor()  const { return diffuse_color_; }
  [[nodiscard]] core::Color GetEmissiveColor() const { return emissive_color_; }
  [[nodiscard]] core::Color GetAmbientColor()  const { return ambient_color_; }
  [[nodiscard]] float       GetShininess()     const { return shininess_; }

 private:
  std::array<std::string, kTextureSlotCount> names_{};
  core::Color diffuse_color_  = core::Color::kWhite;
  core::Color emissive_color_ = core::Color::kTransparent;
  core::Color ambient_color_  = core::Color::kTransparent;
  float       shininess_      = 32.f;
};

}  // namespace renderer
