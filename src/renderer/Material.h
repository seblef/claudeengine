#pragma once

#include <array>
#include <memory>
#include <string>

#include <yaml-cpp/yaml.h>

#include "abstract/ConstantBuffer.h"
#include "abstract/Texture.h"
#include "abstract/VideoDevice.h"
#include "core/Color.h"
#include "renderer/MaterialDesc.h"
#include "renderer/TextureSlot.h"

namespace renderer {

// Surface material: texture slots plus per-material Blinn-Phong color properties.
// Unspecified slots fall back to default textures from data/textures/default/.
//
// The material owns its own ConstantBuffer (UBO slot 3) for color data.
// Calling Set() binds all textures and uploads color properties in one step,
// so the caller does not need to manage any separate constant buffer.
//
// Textures are reference-counted: the Material calls AddRef on construction
// (or SetTexture) and Release on destruction.
class Material {
 public:
  using TextureArray = std::array<abstract::Texture*, kTextureSlotCount>;

  // Loads all default textures; color properties at their defaults.
  explicit Material(abstract::VideoDevice* video);

  // Loads textures and color properties from desc; unset slots fall back to defaults.
  Material(const MaterialDesc& desc, abstract::VideoDevice* video);

  // Loads a material from a YAML file. name is relative to data/materials/.
  Material(const std::string& name, abstract::VideoDevice* video);

  // Loads a material from a pre-parsed YAML node.
  Material(const YAML::Node& yaml, abstract::VideoDevice* video);

  ~Material();

  Material(const Material&) = delete;
  Material& operator=(const Material&) = delete;

  // Binds all textures to their corresponding texture units and uploads
  // color properties (diffuse_color, emissive_color, ambient_color, shininess)
  // to the material constant buffer (UBO slot 3).
  void Set() const;

  // Returns true if this material contributes to the emissive pass:
  // emissive_color.rgb or ambient_color.rgb is non-zero.
  [[nodiscard]] bool IsEmissive() const;

  // Returns the texture at the given slot.
  [[nodiscard]] abstract::Texture* GetTexture(TextureSlot slot) const;

  // Returns the full texture array (read-only).
  [[nodiscard]] const TextureArray& GetTextures() const;

  // Replaces the texture at slot. Releases the old reference; AddRefs the new.
  void SetTexture(TextureSlot slot, abstract::Texture* tex);

  // ---- Color property getters ----------------------------------------------

  [[nodiscard]] core::Color GetDiffuseColor()  const { return diffuse_color_; }
  [[nodiscard]] core::Color GetEmissiveColor() const { return emissive_color_; }
  [[nodiscard]] core::Color GetAmbientColor()  const { return ambient_color_; }
  [[nodiscard]] float       GetShininess()     const { return shininess_; }

 private:
  void LoadDefaults(abstract::VideoDevice* video);
  void LoadFromYaml(const YAML::Node& yaml, abstract::VideoDevice* video);
  void InitColorsCB(abstract::VideoDevice* video);

  // cppcheck-suppress unusedStructMember
  TextureArray textures_{};
  // cppcheck-suppress unusedStructMember
  core::Color  diffuse_color_  = core::Color::kWhite;
  // cppcheck-suppress unusedStructMember
  core::Color  emissive_color_ = core::Color::kTransparent;
  // cppcheck-suppress unusedStructMember
  core::Color  ambient_color_  = core::Color::kTransparent;
  float        shininess_      = 32.f;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<abstract::ConstantBuffer> colors_cb_;
};

}  // namespace renderer
