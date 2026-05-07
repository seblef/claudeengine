#pragma once

#include "renderer/GeometryData.h"
#include "renderer/Material.h"

namespace renderer {

// Container pairing geometry and surface material.
// Does not own either; both must outlive the Mesh.
class Mesh {
 public:
  Mesh(GeometryData* geometry, Material* material);

  [[nodiscard]] GeometryData* GetGeometryData() const;
  void SetGeometryData(GeometryData* geometry);

  [[nodiscard]] Material* GetMaterial() const;
  void SetMaterial(Material* material);

 private:
  // cppcheck-suppress unusedStructMember
  GeometryData* geometry_;
  // cppcheck-suppress unusedStructMember
  Material*     material_;
};

}  // namespace renderer
