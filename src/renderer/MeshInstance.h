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

  [[nodiscard]] Mesh* GetModel() const;

 private:
  // cppcheck-suppress unusedStructMember
  Mesh* model_;
};

}  // namespace renderer
