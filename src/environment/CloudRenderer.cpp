#include "environment/CloudRenderer.h"

#include <cstdint>

#include "abstract/BlendFactor.h"
#include "abstract/BufferUsage.h"
#include "abstract/CompareFunc.h"
#include "abstract/IndexType.h"
#include "core/Vertex3D.h"
#include "core/VertexType.h"

namespace environment {

void CloudRenderer::Build(abstract::VideoDevice* video) {
  video_  = video;
  shader_ = video_->CreateShader("clouds/clouds");

  // Fullscreen quad covering NDC [-1, 1]² at z=0 (VS moves it to the far plane).
  const core::Vertex3D verts[4] = {
    {{-1.f, -1.f, 0.f}, {}, {}, {}, {}},
    {{ 1.f, -1.f, 0.f}, {}, {}, {}, {}},
    {{ 1.f,  1.f, 0.f}, {}, {}, {}, {}},
    {{-1.f,  1.f, 0.f}, {}, {}, {}, {}},
  };
  const uint16_t idx[6] = {0, 1, 2, 0, 2, 3};

  quad_vb_ = video_->CreateVertexBuffer(
      core::VertexType::k3D, 4,
      abstract::BufferUsage::kImmutable, verts);
  quad_ib_ = video_->CreateIndexBuffer(
      abstract::IndexType::kUInt16, 6,
      abstract::BufferUsage::kImmutable, idx);
}

void CloudRenderer::Render(float /*world_time*/, float cloud_density) {
  if (!shader_) return;

  video_->SetDepthWriteEnabled(false);
  video_->SetDepthTestEnabled(true);
  video_->SetDepthFunc(abstract::CompareFunc::kLessEqual);
  video_->SetIndexType(abstract::IndexType::kUInt16);

  // Alpha-blend clouds over the sky colour without adding luminance.
  video_->SetBlendEnabled(true,
                          abstract::BlendFactor::kSrcAlpha,
                          abstract::BlendFactor::kOneMinusSrcAlpha);

  shader_->Activate();
  shader_->SetUniformFloat("cloud_density", cloud_density);

  quad_vb_->Bind();
  quad_ib_->Bind();
  video_->RenderIndexed(6);

  video_->SetBlendEnabled(false);
  video_->SetDepthFunc(abstract::CompareFunc::kLess);
}

void CloudRenderer::Reset() {
  if (shader_) {
    shader_->Release();
    shader_ = nullptr;
  }
  quad_vb_.reset();
  quad_ib_.reset();
  video_ = nullptr;
}

}  // namespace environment
