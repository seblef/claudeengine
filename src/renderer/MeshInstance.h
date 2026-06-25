#pragma once

#include "renderer/Mesh.h"
#include "renderer/Renderable.h"

namespace renderer {

// A renderable instance of a Mesh.
//
// Enqueue() submits this instance to MeshRenderer for the current frame.
// The Mesh must outlive this instance.
class MeshInstance : public Renderable {
 public:
  MeshInstance(Mesh* model,
               const core::Mat4f& world_matrix,
               bool always_visible);

  // Adds this instance to the MeshRenderer queue for the current frame.
  void Enqueue() override;

  // Returns true when the mesh's material has shadow casting enabled.
  // Returns false when no material is assigned.
  [[nodiscard]] bool IsShadowCaster() const override {
    const Material* mat = model_->GetMaterial();
    return mat && mat->GetCastShadow();
  }

  // Enqueues this instance in MeshRenderer's depth queue if it is a shadow caster.
  void EnqueueDepth() override;

  [[nodiscard]] Mesh* GetModel() const;

 private:
  // cppcheck-suppress unusedStructMember
  Mesh* model_;
};

}  // namespace renderer
