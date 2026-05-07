#include "renderer/Mesh.h"

namespace renderer {

Mesh::Mesh(GeometryData* geometry, Material* material)
    : geometry_(geometry), material_(material) {}

GeometryData* Mesh::GetGeometryData() const { return geometry_; }
void Mesh::SetGeometryData(GeometryData* geometry) { geometry_ = geometry; }

Material* Mesh::GetMaterial() const { return material_; }
void Mesh::SetMaterial(Material* material) { material_ = material; }

}  // namespace renderer
