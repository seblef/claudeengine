#include "renderer/MeshInstance.h"

#include "renderer/MeshRenderer.h"

namespace renderer {

MeshInstance::MeshInstance(Mesh* model,
                           const core::Mat4f& world_matrix,
                           bool always_visible)
    : Renderable(model->GetGeometryData()->GetBBox(), world_matrix, always_visible),
      model_(model) {}

void MeshInstance::Enqueue() {
  MeshRenderer::Instance().AddInstance(this);
}

Mesh* MeshInstance::GetModel() const { return model_; }

}  // namespace renderer
