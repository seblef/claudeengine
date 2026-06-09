#include "renderer/Mesh.h"

#include <utility>

namespace renderer {

Mesh::Mesh(GeometryData* geometry, Material* material)
    : geometry_(geometry), materials_({material}) {}

Mesh::Mesh(GeometryData* geometry, std::vector<Material*> materials)
    : geometry_(geometry), materials_(std::move(materials)) {}

GeometryData* Mesh::GetGeometryData() const { return geometry_; }
void Mesh::SetGeometryData(GeometryData* geometry) { geometry_ = geometry; }

Material* Mesh::GetMaterial() const { return materials_[0]; }
void Mesh::SetMaterial(Material* material) { materials_[0] = material; }

int Mesh::GetMaterialCount() const {
  return static_cast<int>(materials_.size());
}

Material* Mesh::GetMaterial(int slot) const { return materials_[slot]; }
void Mesh::SetMaterial(int slot, Material* material) {
  materials_[slot] = material;
}

}  // namespace renderer
