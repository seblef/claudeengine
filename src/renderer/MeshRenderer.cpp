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

  const Material*     current_material = nullptr;
  const GeometryData* current_geo      = nullptr;

  for (const MeshInstance* instance : instances_) {
    const Mesh*         mesh = instance->GetModel();
    const GeometryData* geo  = mesh->GetGeometryData();

    if (geo != current_geo) {
      geo->Set();
      current_geo = geo;
    }

    Renderer::Instance().SetRenderableInfos(instance->GetWorldMatrix());

    const int sub_count = geo->GetSubMeshCount();
    if (sub_count == 0) {
      // Fast path: single material covers the full index range.
      const Material* mat = mesh->GetMaterial();
      if (mat != current_material) {
        mat->Set(Renderer::Instance().GetMaterialInfosCB());
        current_material = mat;
      }
      video_->RenderIndexed(geo->GetNumIndices());
    } else {
      // Multi-material path: one draw call per submesh.
      for (int i = 0; i < sub_count; ++i) {
        const Material* mat = mesh->GetMaterial(i);
        if (mat != current_material) {
          mat->Set(Renderer::Instance().GetMaterialInfosCB());
          current_material = mat;
        }
        const auto& sub = geo->GetSubMesh(i);
        video_->RenderIndexed(static_cast<int>(sub.index_count),
                               static_cast<int>(sub.index_offset));
      }
    }
  }
}

void MeshRenderer::RenderEmissive() {
  emissive_shader_->Activate();

  const Material*     current_material = nullptr;
  const GeometryData* current_geo      = nullptr;

  for (const MeshInstance* instance : instances_) {
    const Mesh*         mesh = instance->GetModel();
    const GeometryData* geo  = mesh->GetGeometryData();

    const int sub_count = geo->GetSubMeshCount();
    if (sub_count == 0) {
      const Material* mat = mesh->GetMaterial();
      if (!mat->IsEmissive()) continue;

      if (geo != current_geo) {
        geo->Set();
        current_geo = geo;
      }
      if (mat != current_material) {
        mat->Set(Renderer::Instance().GetMaterialInfosCB());
        current_material = mat;
      }
      Renderer::Instance().SetRenderableInfos(instance->GetWorldMatrix());
      video_->RenderIndexed(geo->GetNumIndices());
    } else {
      // Multi-material: emit only submeshes whose material is emissive.
      for (int i = 0; i < sub_count; ++i) {
        const Material* mat = mesh->GetMaterial(i);
        if (!mat->IsEmissive()) continue;

        if (geo != current_geo) {
          geo->Set();
          current_geo = geo;
        }
        if (mat != current_material) {
          mat->Set(Renderer::Instance().GetMaterialInfosCB());
          current_material = mat;
        }
        Renderer::Instance().SetRenderableInfos(instance->GetWorldMatrix());
        const auto& sub = geo->GetSubMesh(i);
        video_->RenderIndexed(static_cast<int>(sub.index_count),
                               static_cast<int>(sub.index_offset));
      }
    }
  }
}

void MeshRenderer::RenderDepth() {
  std::sort(depth_instances_.begin(), depth_instances_.end(),
            [](const MeshInstance* a, const MeshInstance* b) {
              return a->GetModel()->GetGeometryData() <
                     b->GetModel()->GetGeometryData();
            });

  depth_shader_->Activate();

  const Material*     current_material = nullptr;
  const GeometryData* current_geo      = nullptr;
  for (const MeshInstance* inst : depth_instances_) {
    const Mesh*         mesh = inst->GetModel();
    const GeometryData* geo  = mesh->GetGeometryData();
    const Material*     mat  = mesh->GetMaterial();

    if (geo != current_geo) {
      geo->Set();
      current_geo = geo;
    }
    if (mat != current_material) {
      mat->Set(Renderer::Instance().GetMaterialInfosCB());
      current_material = mat;
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

  const Material*     current_material = nullptr;
  const GeometryData* current_geo      = nullptr;
  for (const MeshInstance* inst : depth_instances_) {
    const Mesh*         mesh = inst->GetModel();
    const GeometryData* geo  = mesh->GetGeometryData();
    const Material*     mat  = mesh->GetMaterial();

    if (geo != current_geo) {
      geo->Set();
      current_geo = geo;
    }
    if (mat != current_material) {
      mat->Set(Renderer::Instance().GetMaterialInfosCB());
      current_material = mat;
    }
    Renderer::Instance().SetRenderableInfos(inst->GetWorldMatrix());
    video_->RenderIndexed(geo->GetNumIndices());
  }

  depth_instances_.clear();
}

}  // namespace renderer
