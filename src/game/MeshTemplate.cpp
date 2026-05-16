#include "game/MeshTemplate.h"

#include <loguru.hpp>

#include "game/GameMaterial.h"
#include "renderer/MaterialDesc.h"
#include "renderer/MeshLoader.h"

namespace game {

MeshTemplate::MeshTemplate(const std::string& mesh_path, abstract::VideoDevice* video)
    : Resource(mesh_path),
      geometry_(renderer::MeshLoader::LoadGeometry(mesh_path, video)) {
  if (geometry_) {
    material_ = new GameMaterial("__mat_" + mesh_path, renderer::MaterialDesc(), video);
    mesh_ = std::make_unique<renderer::Mesh>(geometry_.get(), material_->GetMaterial());
    initialized_ = true;
  } else {
    LOG_F(ERROR, "MeshTemplate: failed to load mesh '%s'", mesh_path.c_str());
  }
}

MeshTemplate::MeshTemplate(const std::string& id,
                           std::unique_ptr<renderer::GeometryData> geo,
                           GameMaterial* mat)
    : Resource(id),
      geometry_(std::move(geo)),
      material_(mat) {
  material_->AddRef();
  if (geometry_) {
    mesh_ = std::make_unique<renderer::Mesh>(geometry_.get(), material_->GetMaterial());
    initialized_ = true;
  }
}

MeshTemplate::~MeshTemplate() {
  if (material_) material_->Release();
}

void MeshTemplate::SetMaterial(GameMaterial* mat) {
  if (material_) material_->Release();
  material_ = mat;
  material_->AddRef();
  mesh_->SetMaterial(material_->GetMaterial());
}

renderer::Mesh* MeshTemplate::GetMesh() const {
  return mesh_.get();
}

renderer::GeometryData* MeshTemplate::GetGeometry() const {
  return geometry_.get();
}

GameMaterial* MeshTemplate::GetMaterial() const {
  return material_;
}

const core::BBox3& MeshTemplate::GetLocalBBox() const {
  return geometry_->GetBBox();
}

// static
std::map<std::string, MeshTemplate*> MeshTemplate::GetAll() {
  std::map<std::string, MeshTemplate*> result;
  for (const auto& kv : Resource::GetRegistry()) {
    if (kv.first.rfind("__proc_", 0) == 0) continue;
    const auto pos = kv.first.find_last_of("/\\");
    const std::string name = (pos == std::string::npos)
                             ? kv.first
                             : kv.first.substr(pos + 1);
    result[name] = kv.second;
  }
  return result;
}

// static
MeshTemplate* MeshTemplate::GetOrLoad(const std::string& mesh_path,
                                      abstract::VideoDevice* video) {
  MeshTemplate* existing = Get(mesh_path);
  if (existing) {
    existing->AddRef();
    return existing;
  }
  return new MeshTemplate(mesh_path, video);
}

}  // namespace game
