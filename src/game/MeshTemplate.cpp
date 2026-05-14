#include "game/MeshTemplate.h"

#include <loguru.hpp>

#include "renderer/MeshLoader.h"

namespace game {

namespace {
// Generates a unique registry key for each procedurally built mesh template.
std::string GenerateProcId() {
  static int counter = 0;
  return "__proc_" + std::to_string(counter++);
}
}  // namespace

MeshTemplate::MeshTemplate(renderer::RenderableMesh* mesh)
    : Resource(GenerateProcId()),
      mesh_(mesh) {
  initialized_ = true;
}

MeshTemplate::MeshTemplate(const std::string& mesh_path,
                           abstract::VideoDevice* video)
    : Resource(mesh_path),
      mesh_(renderer::MeshLoader::Load(mesh_path, video)) {
  initialized_ = (mesh_ != nullptr);
  if (!initialized_) {
    LOG_F(ERROR, "MeshTemplate: failed to load mesh '%s'", mesh_path.c_str());
  }
}

renderer::RenderableMesh* MeshTemplate::GetRenderableMesh() const {
  return mesh_.get();
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
