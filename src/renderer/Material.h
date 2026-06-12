#pragma once

#include <array>
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
// The material does not own any GPU constant buffer. Callers pass the shared
// material-infos CB (owned by Renderer, slot 3) to Set(), which binds all
// textures and fills the CB atomically.
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

  // Binds all textures to their corresponding texture units and fills
  // colors_cb with diffuse_color, emissive_color, ambient_color, shininess.
  // colors_cb is the shared material-infos buffer owned by Renderer (slot 3).
  void Set(abstract::ConstantBuffer* colors_cb) const;

  // Returns true if this material contributes to the emissive pass:
  // emissive_color.rgb or ambient_color.rgb is non-zero.
  [[nodiscard]] bool IsEmissive() const;

  // Returns false if meshes with this material should be skipped in all shadow
  // passes.  Corresponds to the YAML key `cast_shadow` (default true).
  [[nodiscard]] bool GetCastShadow() const { return cast_shadow_; }

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
  [[nodiscard]] float       GetSpecular()      const { return specular_; }

  // ---- Color property setters ----------------------------------------------

  void SetDiffuseColor(core::Color color)  { diffuse_color_  = color; }
  void SetEmissiveColor(core::Color color) { emissive_color_ = color; }
  void SetAmbientColor(core::Color color)  { ambient_color_  = color; }
  void SetShininess(float shininess)       { shininess_      = shininess; }
  void SetSpecular(float specular)         { specular_       = specular; }

 private:
  void LoadDefaults(abstract::VideoDevice* video);
  void LoadFromYaml(const YAML::Node& yaml, abstract::VideoDevice* video);

  // cppcheck-suppress unusedStructMember
  TextureArray textures_{};
  // cppcheck-suppress unusedStructMember
  core::Color  diffuse_color_  = core::Color::kWhite;
  // cppcheck-suppress unusedStructMember
  core::Color  emissive_color_ = core::Color::kTransparent;
  // cppcheck-suppress unusedStructMember
  core::Color  ambient_color_  = core::Color::kTransparent;
  float        shininess_      = 32.f;
  float        specular_       = 1.f;
  // cppcheck-suppress unusedStructMember
  bool         cast_shadow_    = true;
};

}  // namespace renderer
