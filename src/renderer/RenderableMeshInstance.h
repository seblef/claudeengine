#pragma once

#include <memory>
#include <vector>

#include "core/Mat4f.h"
#include "renderer/MeshInstance.h"
#include "renderer/RenderableMesh.h"
#include "renderer/Renderable.h"

namespace renderer {

// A scene instance of a RenderableMesh.
//
// Participates in the visibility system via its combined bounding box (the
// union of all submesh AABBs transformed by world_matrix). When visible,
// Enqueue() syncs the world matrix to each per-submesh MeshInstance and
// registers them with MeshRenderer for the current frame.
//
// The referenced RenderableMesh must outlive this instance.
class RenderableMeshInstance : public Renderable {
 public:
  RenderableMeshInstance(RenderableMesh* model,
                         const core::Mat4f& world_matrix,
                         bool always_visible);

  void Enqueue() override;

  // Returns true if any sub-instance is a shadow caster.
  [[nodiscard]] bool IsShadowCaster() const override;

  // Renders the depth contribution of each shadow-casting sub-instance.
  void DrawDepth(abstract::VideoDevice* video) override;

  [[nodiscard]] RenderableMesh* GetModel() const;

 private:
  // cppcheck-suppress unusedStructMember
  RenderableMesh*                           model_;
  // cppcheck-suppress unusedStructMember
  std::vector<std::unique_ptr<MeshInstance>> sub_instances_;
};

}  // namespace renderer
