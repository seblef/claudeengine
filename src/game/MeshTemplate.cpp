#include "game/MeshTemplate.h"

#include <loguru.hpp>

#include "renderer/MeshLoader.h"

namespace game {

namespace {
std::string GenerateProcId() {
  static int counter = 0;
  return "__proc_" + std::to_string(counter++);
}
}  // namespace

MeshTemplate::MeshTemplate(std::unique_ptr<renderer::GeometryData> geo,
                           std::unique_ptr<renderer::Material> mat)
    : Resource(GenerateProcId()),
      geometry_(std::move(geo)),
      material_(std::move(mat)) {
  if (geometry_ && material_) {
    mesh_ = std::make_unique<renderer::Mesh>(geometry_.get(), material_.get());
    initialized_ = true;
  }
}

MeshTemplate::MeshTemplate(const std::string& mesh_path,
                           abstract::VideoDevice* video)
    : Resource(mesh_path),
      geometry_(renderer::MeshLoader::LoadGeometry(mesh_path, video)) {
  if (geometry_) {
    material_ = std::make_unique<renderer::Material>(video);
    mesh_ = std::make_unique<renderer::Mesh>(geometry_.get(), material_.get());
    initialized_ = true;
  } else {
    LOG_F(ERROR, "MeshTemplate: failed to load mesh '%s'", mesh_path.c_str());
  }
}

renderer::Mesh* MeshTemplate::GetMesh() const {
  return mesh_.get();
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
