#include "renderer/Material.h"

#include "core/Config.h"
#include "core/YamlUtils.h"

#include <loguru.hpp>

namespace renderer {

namespace {

constexpr const char* kDefaultPaths[kTextureSlotCount] = {
    "default/diffuse.png",      // kDiffuse
    "default/normal.png",       // kNormal
    "default/specular.png",     // kSpecular
    "default/emissive.png",     // kEmissive
    "default/environment.png",  // kEnvironment
};

const std::pair<const char*, TextureSlot> kSlotMap[] = {
    {"diffuse",     TextureSlot::kDiffuse},
    {"normal",      TextureSlot::kNormal},
    {"specular",    TextureSlot::kSpecular},
    {"emissive",    TextureSlot::kEmissive},
    {"environment", TextureSlot::kEnvironment},
};

}  // namespace

Material::Material(abstract::VideoDevice* video) {
  LoadDefaults(video);
}

Material::Material(TextureArray textures, abstract::VideoDevice* video)
    : textures_(textures) {
  for (auto* tex : textures_) {
    if (tex) tex->AddRef();
  }
  for (int i = 0; i < kTextureSlotCount; ++i) {
    if (!textures_[i])
      textures_[i] = video->CreateTexture(kDefaultPaths[i]);
  }
}

Material::Material(const std::string& name, abstract::VideoDevice* video) {
  const auto path = core::Config::GetDataFolder() / "materials" / name;
  try {
    const YAML::Node root = core::LoadYamlFile(path);
    LoadFromYaml(root, video);
  } catch (const std::exception& e) {
    LOG_F(ERROR, "Material '%s': load error: %s", name.c_str(), e.what());
    LoadDefaults(video);
  }
}

Material::Material(const YAML::Node& yaml, abstract::VideoDevice* video) {
  LoadFromYaml(yaml, video);
}

Material::~Material() {
  for (auto* tex : textures_) {
    if (tex) tex->Release();
  }
}

void Material::Set() const {
  for (int i = 0; i < kTextureSlotCount; ++i) {
    if (textures_[i]) textures_[i]->Bind(i);
  }
}

abstract::Texture* Material::GetTexture(TextureSlot slot) const {
  return textures_[static_cast<int>(slot)];
}

const Material::TextureArray& Material::GetTextures() const {
  return textures_;
}

void Material::SetTexture(TextureSlot slot, abstract::Texture* tex) {
  auto& entry = textures_[static_cast<int>(slot)];
  if (entry) entry->Release();
  entry = tex;
  if (entry) entry->AddRef();
}

void Material::LoadDefaults(abstract::VideoDevice* video) {
  for (int i = 0; i < kTextureSlotCount; ++i) {
    textures_[i] = video->CreateTexture(kDefaultPaths[i]);
    if (!textures_[i]) {
      LOG_F(ERROR, "Material: failed to load default texture for slot %d", i);
    }
  }
}

void Material::LoadFromYaml(const YAML::Node& yaml, abstract::VideoDevice* video) {
  LoadDefaults(video);
  if (!yaml["textures"]) return;
  const YAML::Node& tex_node = yaml["textures"];
  for (const auto& [key, slot] : kSlotMap) {
    if (!tex_node[key]) continue;
    const std::string tex_name = tex_node[key].as<std::string>();
    abstract::Texture* tex = video->CreateTexture(tex_name);
    if (!tex) {
      LOG_F(WARNING, "Material: texture '%s' not found, keeping default",
            tex_name.c_str());
      continue;
    }
    auto& entry = textures_[static_cast<int>(slot)];
    if (entry) entry->Release();
    entry = tex;
  }
}

}  // namespace renderer
