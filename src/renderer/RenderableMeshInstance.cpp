#include "renderer/RenderableMeshInstance.h"

#include "renderer/MeshRenderer.h"

namespace renderer {

RenderableMeshInstance::RenderableMeshInstance(RenderableMesh* model,
                                               const core::Mat4f& world_matrix,
                                               bool always_visible)
    : Renderable(model->GetLocalBBox(), world_matrix, always_visible),
      model_(model) {
  sub_instances_.reserve(model->GetSubmeshCount());
  for (size_t i = 0; i < model->GetSubmeshCount(); ++i) {
    sub_instances_.push_back(
        std::make_unique<MeshInstance>(model->GetSubmesh(i), world_matrix, false));
  }
}

void RenderableMeshInstance::Enqueue() {
  const core::Mat4f& wm = GetWorldMatrix();
  for (auto& inst : sub_instances_) {
    inst->SetWorldMatrix(wm);
    MeshRenderer::Instance().AddInstance(inst.get());
  }
}

RenderableMesh* RenderableMeshInstance::GetModel() const { return model_; }

}  // namespace renderer
