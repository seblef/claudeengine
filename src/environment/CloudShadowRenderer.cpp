#include "environment/CloudShadowRenderer.h"

#include <array>
#include <cstdint>

#include "abstract/BufferUsage.h"
#include "abstract/CompareFunc.h"
#include "abstract/IndexType.h"
#include "abstract/TextureFormat.h"
#include "core/Color.h"
#include "core/Vertex3D.h"
#include "core/VertexType.h"

namespace environment {

namespace {
constexpr int kShadowMapSize = 1024;
}  // namespace

void CloudShadowRenderer::Build(abstract::VideoDevice* video) {
  video_  = video;
  shader_ = video_->CreateShader("clouds/cloud_shadow");

  shadow_rt_ = video_->CreateRenderTarget(
      kShadowMapSize, kShadowMapSize, abstract::TextureFormat::kR16F);
  std::array<abstract::RenderTarget*, 1> colors = {shadow_rt_.get()};
  shadow_fbo_ = video_->CreateRenderTargetGroup(colors, nullptr);

  // Clear once to zero (no shadow).
  shadow_fbo_->BindForWriting();
  video_->ClearRenderTargets(core::Color::kBlack);
  shadow_fbo_->UnbindForWriting();

  // Fullscreen quad covering NDC [-1, 1]² (VS places it at z = 0).
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

void CloudShadowRenderer::Render(float cloud_density) {
  if (!shader_) return;

  video_->SetDepthTestEnabled(false);
  video_->SetDepthWriteEnabled(false);
  video_->SetBlendEnabled(false);
  video_->SetIndexType(abstract::IndexType::kUInt16);

  shadow_fbo_->BindForWriting();
  video_->SetViewport(0, 0, kShadowMapSize, kShadowMapSize);

  shader_->Activate();
  shader_->SetUniformFloat("cloud_density",         cloud_density);
  shader_->SetUniformFloat("cloud_shadow_coverage", coverage_radius_);

  quad_vb_->Bind();
  quad_ib_->Bind();
  video_->RenderIndexed(6);

  shadow_fbo_->UnbindForWriting();
}

void CloudShadowRenderer::Reset() {
  if (shader_) {
    shader_->Release();
    shader_ = nullptr;
  }
  shadow_fbo_.reset();
  shadow_rt_.reset();
  quad_vb_.reset();
  quad_ib_.reset();
  video_ = nullptr;
}

}  // namespace environment
