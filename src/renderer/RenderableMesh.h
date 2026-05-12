#pragma once

#include <memory>
#include <vector>

#include "core/BBox3.h"
#include "renderer/GeometryData.h"
#include "renderer/Material.h"
#include "renderer/Mesh.h"

namespace renderer {

// A renderable model composed of one or more submeshes.
//
// Each submesh pairs a GeometryData with a Material. RenderableMesh owns both.
// The local bounding box is the axis-aligned union of all submesh bounding boxes.
//
// Typical usage: build via MeshLoader::Load, then create RenderableMeshInstance(s)
// that reference this model and share its geometry across multiple scene placements.
class RenderableMesh {
 public:
  // Adds a submesh; ownership of geo and mat is transferred.
  // The combined local bounding box is expanded to include geo's bounding box.
  void AddSubmesh(std::unique_ptr<GeometryData> geo,
                  std::unique_ptr<Material> mat);

  [[nodiscard]] size_t             GetSubmeshCount() const;
  [[nodiscard]] Mesh*              GetSubmesh(size_t idx) const;
  [[nodiscard]] const core::BBox3& GetLocalBBox()    const;

 private:
  // cppcheck-suppress unusedStructMember
  std::vector<std::unique_ptr<GeometryData>> geometries_;
  // cppcheck-suppress unusedStructMember
  std::vector<std::unique_ptr<Material>>     materials_;
  // cppcheck-suppress unusedStructMember
  std::vector<std::unique_ptr<Mesh>>         submeshes_;
  // cppcheck-suppress unusedStructMember
  core::BBox3                                local_bbox_;
  // cppcheck-suppress unusedStructMember
  bool                                       bbox_initialized_ = false;
};

}  // namespace renderer
