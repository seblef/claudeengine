#pragma once

#include <array>
#include <string>

#include <yaml-cpp/yaml.h>

#include "abstract/Texture.h"
#include "abstract/VideoDevice.h"
#include "renderer/MaterialDesc.h"
#include "renderer/TextureSlot.h"

namespace renderer {

// Surface material. Currently manages one texture per slot.
// Unspecified slots fall back to default textures from data/textures/default/.
//
// Textures are reference-counted: the Material calls AddRef on construction
// (or SetTexture) and Release on destruction.
class Material {
 public:
  using TextureArray = std::array<abstract::Texture*, kTextureSlotCount>;

  // Loads all default textures.
  explicit Material(abstract::VideoDevice* video);

  // Loads textures by name from desc; unset slots fall back to defaults.
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

  // Returns the texture at the given slot.
  [[nodiscard]] abstract::Texture* GetTexture(TextureSlot slot) const;

  // Returns the full texture array (read-only).
  [[nodiscard]] const TextureArray& GetTextures() const;

  // Replaces the texture at slot. Releases the old reference; AddRefs the new.
  void SetTexture(TextureSlot slot, abstract::Texture* tex);

 private:
  void LoadDefaults(abstract::VideoDevice* video);
  void LoadFromYaml(const YAML::Node& yaml, abstract::VideoDevice* video);

  // cppcheck-suppress unusedStructMember
  TextureArray textures_{};
};

}  // namespace renderer
