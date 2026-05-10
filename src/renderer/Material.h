#pragma once

#include <array>
#include <string>

#include <yaml-cpp/yaml.h>

#include "abstract/ConstantBuffer.h"
#include "abstract/Texture.h"
#include "abstract/VideoDevice.h"
#include "core/Vec3f.h"
#include "renderer/MaterialDesc.h"
#include "renderer/TextureSlot.h"

namespace renderer {

// Surface material: texture slots plus per-material color properties.
// Unspecified slots fall back to default textures from data/textures/default/.
//
// Textures are reference-counted: the Material calls AddRef on construction
// (or SetTexture) and Release on destruction.
//
// Color properties (diffuse_color, emissive_color, ambient_color, shininess)
// are uploaded to UBO slot 3 each time the material is bound for rendering.
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

  // Binds each texture to its corresponding texture unit.
  void Set() const;

  // Uploads diffuse_color, emissive_color, ambient_color, shininess into cb.
  void UploadColors(abstract::ConstantBuffer* cb) const;

  // Returns true if this material contributes to the emissive pass:
  // emissive_color or ambient_color is non-zero.
  [[nodiscard]] bool IsEmissive() const;

  // Returns the texture at the given slot.
  [[nodiscard]] abstract::Texture* GetTexture(TextureSlot slot) const;

  // Returns the full texture array (read-only).
  [[nodiscard]] const TextureArray& GetTextures() const;

  // Replaces the texture at slot. Releases the old reference; AddRefs the new.
  void SetTexture(TextureSlot slot, abstract::Texture* tex);

  // ---- Color property getters ----------------------------------------------

  [[nodiscard]] core::Vec3f GetDiffuseColor()  const { return diffuse_color_; }
  [[nodiscard]] core::Vec3f GetEmissiveColor() const { return emissive_color_; }
  [[nodiscard]] core::Vec3f GetAmbientColor()  const { return ambient_color_; }
  [[nodiscard]] float       GetShininess()     const { return shininess_; }

 private:
  void LoadDefaults(abstract::VideoDevice* video);
  void LoadFromYaml(const YAML::Node& yaml, abstract::VideoDevice* video);

  // cppcheck-suppress unusedStructMember
  TextureArray textures_{};
  // cppcheck-suppress unusedStructMember
  core::Vec3f  diffuse_color_  = {1.f, 1.f, 1.f};
  // cppcheck-suppress unusedStructMember
  core::Vec3f  emissive_color_ = {0.f, 0.f, 0.f};
  // cppcheck-suppress unusedStructMember
  core::Vec3f  ambient_color_  = {0.f, 0.f, 0.f};
  float        shininess_      = 32.f;
};

}  // namespace renderer
