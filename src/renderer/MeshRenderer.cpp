#include "renderer/MeshRenderer.h"

#include "renderer/Mesh.h"
#include "renderer/Renderer.h"

namespace renderer {

MeshRenderer::MeshRenderer(abstract::VideoDevice* video)
    : ObjectRenderer<MeshInstance>("geometry/mesh_base", video) {}

void MeshRenderer::Render() {
  shader_->Activate();

  const Material* current_material = nullptr;
  const Mesh*     current_mesh     = nullptr;

  for (const MeshInstance* instance : instances_) {
    Mesh*     mesh = instance->GetModel();
    Material* mat  = mesh->GetMaterial();

    if (mat != current_material) {
      mat->Set();
      current_material = mat;
    }

    if (mesh != current_mesh) {
      mesh->GetGeometryData()->Set();
      current_mesh = mesh;
    }

    Renderer::Instance().SetRenderableInfos(instance->GetWorldMatrix());
    video_->RenderIndexed(mesh->GetGeometryData()->GetNumIndices());
  }
}

}  // namespace renderer
