#include "renderer/Material.h"

#include "core/Config.h"
#include "core/YamlSerialiser.h"
#include "renderer/MaterialInfos.h"

#include <loguru.hpp>

namespace renderer {

namespace {

constexpr const char* kDefaultPaths[kTextureSlotCount] = {
    "default/diffuse.dds",      // kDiffuse
    "default/normal.dds",       // kNormal
    "default/specular.dds",     // kSpecular
    "default/emissive.dds",     // kEmissive
    "default/environment.dds",  // kEnvironment
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

Material::Material(const MaterialDesc& desc, abstract::VideoDevice* video) {
  for (int i = 0; i < kTextureSlotCount; ++i) {
    const std::string& name = desc.Get(static_cast<TextureSlot>(i));
    if (!name.empty())
      textures_[i] = video->CreateTexture(name);
    if (!textures_[i]) {
      if (!name.empty())
        LOG_F(WARNING, "Material: texture '%s' not found, using default", name.c_str());
      textures_[i] = video->CreateTexture(kDefaultPaths[i]);
    }
  }
  diffuse_color_  = desc.GetDiffuseColor();
  emissive_color_ = desc.GetEmissiveColor();
  ambient_color_  = desc.GetAmbientColor();
  shininess_      = desc.GetShininess();
  specular_       = desc.GetSpecular();
  cast_shadow_    = desc.GetCastShadow();
  alpha_mask_     = desc.GetAlphaMask();
}

Material::Material(const std::string& name, abstract::VideoDevice* video) {
  const auto path = core::Config::GetDataFolder() / "materials" / name;
  try {
    const YAML::Node root = core::LoadYamlFile(path);
    // Support both the new format (rendering: section) and the old flat format.
    LoadFromYaml(root["rendering"] ? root["rendering"] : root, video);
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

void Material::Set(abstract::ConstantBuffer* colors_cb) const {
  for (int i = 0; i < kTextureSlotCount; ++i) {
    if (textures_[i]) textures_[i]->Bind(i);
  }

  MaterialInfos mi;
  mi.diffuse_color  = diffuse_color_;
  mi.emissive_color = emissive_color_;
  mi.ambient_color  = ambient_color_;
  mi.shininess      = shininess_;
  mi.specular       = specular_;
  mi.alpha_mask     = alpha_mask_ ? 1 : 0;
  colors_cb->Fill(&mi);
}

bool Material::IsEmissive() const {
  return (emissive_color_.r != 0.f || emissive_color_.g != 0.f ||
          emissive_color_.b != 0.f ||
          ambient_color_.r  != 0.f || ambient_color_.g  != 0.f ||
          ambient_color_.b  != 0.f);
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

  if (yaml["textures"]) {
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

  // New format: flat color keys at the node level.
  if (yaml["diffuse_color"])  diffuse_color_  = core::ParseColor(yaml["diffuse_color"]);
  if (yaml["emissive_color"]) emissive_color_ = core::ParseColor(yaml["emissive_color"]);
  if (yaml["ambient_color"])  ambient_color_  = core::ParseColor(yaml["ambient_color"]);
  if (yaml["shininess"])      shininess_      = yaml["shininess"].as<float>();
  if (yaml["specular"])       specular_       = yaml["specular"].as<float>();

  // Old format: colors: sub-node (backward compatibility for legacy files).
  if (yaml["colors"]) {
    const YAML::Node& c = yaml["colors"];
    if (c["diffuse"])   diffuse_color_  = core::ParseColor(c["diffuse"]);
    if (c["emissive"])  emissive_color_ = core::ParseColor(c["emissive"]);
    if (c["ambient"])   ambient_color_  = core::ParseColor(c["ambient"]);
    if (c["shininess"]) shininess_      = c["shininess"].as<float>();
  }

  if (yaml["cast_shadow"]) cast_shadow_ = yaml["cast_shadow"].as<bool>();
  if (yaml["alpha_mask"])  alpha_mask_  = yaml["alpha_mask"].as<bool>();
}

}  // namespace renderer
