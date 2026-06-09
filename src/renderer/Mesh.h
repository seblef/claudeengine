#pragma once

#include <vector>

#include "renderer/GeometryData.h"
#include "renderer/Material.h"

namespace renderer {

// Container pairing geometry with one or more surface materials.
//
// A single-slot Mesh (legacy) holds exactly one Material at index 0.
// A multi-material Mesh holds one Material per submesh slot; slot i maps
// to GeometryData::GetSubMesh(i) and must be bound before each draw call
// for that range.
//
// Does not own either geometry or materials; both must outlive the Mesh.
class Mesh {
 public:
  // Single-material constructor.  Initialises slot 0 with material.
  Mesh(GeometryData* geometry, Material* material);

  // Multi-material constructor.  materials.size() must match the geometry's
  // submesh count (GeometryData::GetSubMeshCount()).
  Mesh(GeometryData* geometry, std::vector<Material*> materials);

  [[nodiscard]] GeometryData* GetGeometryData() const;
  void SetGeometryData(GeometryData* geometry);

  // Slot-0 accessors — kept for backward compatibility with single-material code.
  [[nodiscard]] Material* GetMaterial() const;
  void SetMaterial(Material* material);

  // Per-slot accessors for multi-material meshes.
  [[nodiscard]] int       GetMaterialCount()      const;
  [[nodiscard]] Material* GetMaterial(int slot)   const;
  void                    SetMaterial(int slot, Material* material);

 private:
  // cppcheck-suppress unusedStructMember
  GeometryData*          geometry_;
  // cppcheck-suppress unusedStructMember
  std::vector<Material*> materials_;
};

}  // namespace renderer
