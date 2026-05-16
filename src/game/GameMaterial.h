#pragma once

#include <memory>
#include <string>

#include <yaml-cpp/yaml.h>

#include "abstract/VideoDevice.h"
#include "core/Resource.h"
#include "renderer/Material.h"
#include "renderer/MaterialDesc.h"

namespace game {

// Game-layer ref-counted resource that owns a renderer::Material.
//
// GameMaterial is the single authority for rendering material state at the
// game layer, enabling the editor and game code to work with materials without
// depending on renderer internals directly.
//
// Keyed by name (basename without extension, relative to data/materials/).
// Always obtain file-backed instances via GetOrLoad(); use the MaterialDesc
// constructor only for programmatic / procedural materials.
//
// Lifecycle:
//   GameMaterial* m = GameMaterial::GetOrLoad("demo", video);
//   // ... use m->GetMaterial() ...
//   m->Release();   // destroys when ref-count reaches zero
class GameMaterial : public core::Resource<std::string, GameMaterial> {
 public:
  // Loads from data/materials/{name}.yaml (expects a rendering: top-level key).
  GameMaterial(const std::string& name, abstract::VideoDevice* video);

  // Constructs from a pre-parsed YAML root node (the node must contain a
  // rendering: key formatted as in the YAML material format).
  GameMaterial(const std::string& name, const YAML::Node& yaml,
               abstract::VideoDevice* video);

  // Constructs programmatically from a MaterialDesc.
  GameMaterial(const std::string& name, const renderer::MaterialDesc& desc,
               abstract::VideoDevice* video);

  // Returns the existing instance (AddRef'd) or loads a new one from disk.
  [[nodiscard]] static GameMaterial* GetOrLoad(const std::string& name,
                                               abstract::VideoDevice* video);

  // Returns the underlying renderer material. Never null after successful init.
  [[nodiscard]] renderer::Material* GetMaterial() const;

 private:
  void InitFromYaml(const YAML::Node& root, abstract::VideoDevice* video);

  // cppcheck-suppress unusedStructMember
  std::unique_ptr<renderer::Material> material_;
};

}  // namespace game
