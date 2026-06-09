#include "game/MeshTemplate.h"

#include <algorithm>

#include <loguru.hpp>

#include "game/GameMaterial.h"
#include "renderer/MaterialDesc.h"
#include "renderer/MeshLoader.h"

namespace game {

// static helper: create a default untextured material for a given id.
static GameMaterial* MakeDefaultMat(const std::string& id,
                                    abstract::VideoDevice* video) {
  return new GameMaterial(id, renderer::MaterialDesc(), video);
}

void MeshTemplate::InitMaterials(int sub_count, const std::string& mesh_path,
                                 abstract::VideoDevice* video,
                                 GameMaterial* override_mat) {
  if (sub_count > 0) {
    materials_.resize(sub_count);
    for (int i = 0; i < sub_count; ++i) {
      const std::string& name = geometry_->GetSubMesh(i).material_name;
      if (!name.empty()) {
        materials_[i] = GameMaterial::GetOrLoad(name, video);
      } else {
        materials_[i] = MakeDefaultMat(
            "__mat_" + mesh_path + "_" + std::to_string(i), video);
      }
    }
    if (override_mat) {
      materials_[0]->Release();
      materials_[0] = override_mat;
      override_mat->AddRef();
    }
  } else {
    // Legacy single-material path.
    if (override_mat) {
      materials_.push_back(override_mat);
      override_mat->AddRef();
    } else {
      materials_.push_back(MakeDefaultMat("__mat_" + mesh_path, video));
    }
  }
}

MeshTemplate::MeshTemplate(const std::string& mesh_path,
                           abstract::VideoDevice* video,
                           GameMaterial* mat)
    : Resource(mesh_path) {
  auto result = renderer::MeshLoader::Load(mesh_path, video);
  if (result) {
    geometry_      = std::move(result->geometry);
    cpu_positions_ = std::move(result->cpu.positions);
    cpu_indices_   = std::move(result->cpu.indices);

    InitMaterials(geometry_->GetSubMeshCount(), mesh_path, video, mat);

    // Build the material pointer list for the Mesh.
    std::vector<renderer::Material*> mat_ptrs(materials_.size());
    std::transform(materials_.begin(), materials_.end(), mat_ptrs.begin(),
                   [](const GameMaterial* gm) { return gm->GetMaterial(); });

    mesh_ = std::make_unique<renderer::Mesh>(geometry_.get(),
                                             std::move(mat_ptrs));
    initialized_ = true;
  } else {
    LOG_F(ERROR, "MeshTemplate: failed to load mesh '%s'", mesh_path.c_str());
  }
}

MeshTemplate::MeshTemplate(const std::string& id,
                           std::unique_ptr<renderer::GeometryData> geo,
                           GameMaterial* mat)
    : Resource(id),
      geometry_(std::move(geo)) {
  if (!geometry_) return;

  const int sub_count = geometry_->GetSubMeshCount();
  if (sub_count > 0) {
    // Allocate one slot per submesh; all start pointing at mat.
    materials_.resize(sub_count, mat);
    for (auto* m : materials_) m->AddRef();

    std::vector<renderer::Material*> mat_ptrs(sub_count, mat->GetMaterial());
    mesh_ = std::make_unique<renderer::Mesh>(geometry_.get(),
                                             std::move(mat_ptrs));
  } else {
    mat->AddRef();
    materials_.push_back(mat);
    mesh_ = std::make_unique<renderer::Mesh>(geometry_.get(),
                                             mat->GetMaterial());
  }
  initialized_ = true;
}

MeshTemplate::~MeshTemplate() {
  for (GameMaterial* m : materials_) {
    if (m) m->Release();
  }
}

void MeshTemplate::SetMaterial(GameMaterial* mat) {
  SetMaterial(0, mat);
}

void MeshTemplate::SetMaterial(int slot, GameMaterial* mat) {
  if (materials_[slot]) materials_[slot]->Release();
  materials_[slot] = mat;
  mat->AddRef();
  mesh_->SetMaterial(slot, mat->GetMaterial());
}

renderer::Mesh* MeshTemplate::GetMesh() const {
  return mesh_.get();
}

renderer::GeometryData* MeshTemplate::GetGeometry() const {
  return geometry_.get();
}

GameMaterial* MeshTemplate::GetMaterial() const {
  return materials_[0];
}

GameMaterial* MeshTemplate::GetMaterial(int slot) const {
  return materials_[slot];
}

int MeshTemplate::GetMaterialCount() const {
  return static_cast<int>(materials_.size());
}

const core::BBox3& MeshTemplate::GetLocalBBox() const {
  return geometry_->GetBBox();
}

const std::vector<core::Vec3f>& MeshTemplate::GetCPUPositions() const {
  return cpu_positions_;
}

const std::vector<uint32_t>& MeshTemplate::GetCPUIndices() const {
  return cpu_indices_;
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
                                      abstract::VideoDevice* video,
                                      GameMaterial* mat) {
  MeshTemplate* existing = Get(mesh_path);
  if (existing) {
    existing->AddRef();
    return existing;
  }
  return new MeshTemplate(mesh_path, video, mat);
}

}  // namespace game
