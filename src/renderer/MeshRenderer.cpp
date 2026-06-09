#include "renderer/MeshRenderer.h"

#include <algorithm>

#include "renderer/GeometryData.h"
#include "renderer/Mesh.h"
#include "renderer/Renderer.h"

namespace renderer {

MeshRenderer::MeshRenderer(abstract::VideoDevice* video)
    : ObjectRenderer<MeshInstance>("geometry/gbuffer", video),
      emissive_shader_(video->CreateShader("geometry/emissive")),
      cube_depth_shader_(video->CreateShader("shadow_depth_cube")) {
  depth_shader_ = video->CreateShader("shadow_depth");
}

MeshRenderer::~MeshRenderer() {
  emissive_shader_->Release();
  cube_depth_shader_->Release();
}

void MeshRenderer::Render() {
  shader_->Activate();
  video_->SetIndexType(abstract::IndexType::kUInt32);

  const Material* current_material = nullptr;
  const Mesh*     current_mesh     = nullptr;

  for (const MeshInstance* instance : instances_) {
    const Mesh*     mesh = instance->GetModel();
    const Material* mat  = mesh->GetMaterial();

    if (mat != current_material) {
      mat->Set(Renderer::Instance().GetMaterialInfosCB());
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

void MeshRenderer::RenderEmissive() {
  emissive_shader_->Activate();

  const Material* current_material = nullptr;
  const Mesh*     current_mesh     = nullptr;

  for (const MeshInstance* instance : instances_) {
    const Mesh*     mesh = instance->GetModel();
    const Material* mat  = mesh->GetMaterial();

    if (!mat->IsEmissive()) continue;

    if (mat != current_material) {
      mat->Set(Renderer::Instance().GetMaterialInfosCB());
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

void MeshRenderer::RenderDepth() {
  std::sort(depth_instances_.begin(), depth_instances_.end(),
            [](const MeshInstance* a, const MeshInstance* b) {
              return a->GetModel()->GetGeometryData() <
                     b->GetModel()->GetGeometryData();
            });

  depth_shader_->Activate();

  const GeometryData* current_geo = nullptr;
  for (const MeshInstance* inst : depth_instances_) {
    const GeometryData* geo = inst->GetModel()->GetGeometryData();
    if (geo != current_geo) {
      geo->Set();
      current_geo = geo;
    }
    Renderer::Instance().SetRenderableInfos(inst->GetWorldMatrix());
    video_->RenderIndexed(geo->GetNumIndices());
  }

  depth_instances_.clear();
}

void MeshRenderer::RenderDepthCube() {
  std::sort(depth_instances_.begin(), depth_instances_.end(),
            [](const MeshInstance* a, const MeshInstance* b) {
              return a->GetModel()->GetGeometryData() <
                     b->GetModel()->GetGeometryData();
            });

  cube_depth_shader_->Activate();

  const GeometryData* current_geo = nullptr;
  for (const MeshInstance* inst : depth_instances_) {
    const GeometryData* geo = inst->GetModel()->GetGeometryData();
    if (geo != current_geo) {
      geo->Set();
      current_geo = geo;
    }
    Renderer::Instance().SetRenderableInfos(inst->GetWorldMatrix());
    video_->RenderIndexed(geo->GetNumIndices());
  }

  depth_instances_.clear();
}

}  // namespace renderer
