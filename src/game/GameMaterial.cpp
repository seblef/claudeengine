#include "game/GameMaterial.h"

#include <loguru.hpp>

#include "core/Config.h"
#include "core/YamlSerialiser.h"

namespace game {

GameMaterial::GameMaterial(const std::string& name, abstract::VideoDevice* video)
    : Resource(name) {
  const auto path =
      core::Config::GetDataFolder() / "materials" / (name + ".yaml");
  try {
    const YAML::Node root = core::LoadYamlFile(path);
    InitFromYaml(root, video);
  } catch (const std::exception& e) {
    LOG_F(ERROR, "GameMaterial '%s': load error: %s", name.c_str(), e.what());
    material_ = std::make_unique<renderer::Material>(video);
    initialized_ = true;
  }
}

GameMaterial::GameMaterial(const std::string& name, const YAML::Node& yaml,
                           abstract::VideoDevice* video)
    : Resource(name) {
  InitFromYaml(yaml, video);
}

GameMaterial::GameMaterial(const std::string& name,
                           const renderer::MaterialDesc& desc,
                           abstract::VideoDevice* video)
    : Resource(name),
      material_(std::make_unique<renderer::Material>(desc, video)) {
  initialized_ = true;
}

// static
GameMaterial* GameMaterial::GetOrLoad(const std::string& name,
                                      abstract::VideoDevice* video) {
  GameMaterial* existing = Get(name);
  if (existing) {
    existing->AddRef();
    return existing;
  }
  return new GameMaterial(name, video);
}

renderer::Material* GameMaterial::GetMaterial() const {
  return material_.get();
}

void GameMaterial::InitFromYaml(const YAML::Node& root,
                                abstract::VideoDevice* video) {
  if (!root["rendering"]) {
    LOG_F(WARNING, "GameMaterial: YAML has no 'rendering' section, using defaults");
    material_ = std::make_unique<renderer::Material>(video);
    initialized_ = true;
    return;
  }
  material_ =
      std::make_unique<renderer::Material>(root["rendering"], video);
  initialized_ = true;
}

}  // namespace game
