#include "game/MeshTemplate.h"

#include <loguru.hpp>

#include "core/YamlUtils.h"
#include "renderer/MeshLoader.h"

namespace game {

MeshTemplate::MeshTemplate(const std::string& mesh_path,
                           abstract::VideoDevice* video,
                           const renderer::MaterialDesc& desc)
    : Resource(mesh_path),
      mesh_(renderer::MeshLoader::Load(mesh_path, video, desc)) {
  initialized_ = (mesh_ != nullptr);
  if (!initialized_) {
    LOG_F(ERROR, "MeshTemplate: failed to load mesh '%s'", mesh_path.c_str());
  }
}

MeshTemplate::MeshTemplate(const std::string& mesh_path,
                           abstract::VideoDevice* video,
                           const std::string& material_file_path)
    : MeshTemplate(mesh_path, video, LoadMaterialDesc(material_file_path)) {}

renderer::RenderableMesh* MeshTemplate::GetRenderableMesh() const {
  return mesh_.get();
}

// static
MeshTemplate* MeshTemplate::GetOrLoad(const std::string& mesh_path,
                                      abstract::VideoDevice* video,
                                      const renderer::MaterialDesc& desc) {
  MeshTemplate* existing = Get(mesh_path);
  if (existing) {
    existing->AddRef();
    return existing;
  }
  return new MeshTemplate(mesh_path, video, desc);
}

// static
MeshTemplate* MeshTemplate::GetOrLoad(const std::string& mesh_path,
                                      abstract::VideoDevice* video,
                                      const std::string& material_file_path) {
  MeshTemplate* existing = Get(mesh_path);
  if (existing) {
    existing->AddRef();
    return existing;
  }
  return new MeshTemplate(mesh_path, video, material_file_path);
}

// static
renderer::MaterialDesc MeshTemplate::LoadMaterialDesc(
    const std::string& material_file_path) {
  renderer::MaterialDesc desc;
  try {
    const YAML::Node root = core::LoadYamlFile(material_file_path);
    if (const YAML::Node textures = root["textures"]) {
      if (textures["diffuse"])
        desc.SetDiffuse(textures["diffuse"].as<std::string>());
      if (textures["normal"])
        desc.SetNormal(textures["normal"].as<std::string>());
      if (textures["specular"])
        desc.SetSpecular(textures["specular"].as<std::string>());
      if (textures["emissive"])
        desc.SetEmissive(textures["emissive"].as<std::string>());
      if (textures["environment"])
        desc.SetEnvironment(textures["environment"].as<std::string>());
    }
  } catch (const std::exception& e) {
    LOG_F(ERROR, "MeshTemplate: failed to load material '%s': %s",
          material_file_path.c_str(), e.what());
  }
  return desc;
}

}  // namespace game
