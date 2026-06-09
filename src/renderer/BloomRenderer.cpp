#include "renderer/BloomRenderer.h"

#include <array>

#include "abstract/BlendFactor.h"
#include "abstract/TextureFormat.h"
#include "core/Color.h"
#include "renderer/GeometryUtils.h"

namespace renderer {

void BloomRenderer::Create(abstract::VideoDevice* video, int width, int height) {
  video_             = video;
  downsample_shader_ = video_->CreateShader("bloom/bloom_downsample");
  upsample_shader_   = video_->CreateShader("bloom/bloom_upsample");
  quad_              = CreateQuad(video_);

  null_rt_ = video_->CreateRenderTarget(1, 1,
                                        abstract::TextureFormat::kG11R11B10F);
  std::array<abstract::RenderTarget*, 1> null_colors = {null_rt_.get()};
  null_fbo_ = video_->CreateRenderTargetGroup(null_colors, nullptr);

  // Clear the 1×1 black no-op RT once.
  null_fbo_->BindForWriting();
  video_->ClearRenderTargets(core::Color::kBlack);
  null_fbo_->UnbindForWriting();

  CreateLevels(width, height);
  bloom_texture_ = null_rt_.get();
}

void BloomRenderer::CreateLevels(int width, int height) {
  int w = width  / 2;
  int h = height / 2;
  for (int i = 0; i < kLevelCount; ++i) {
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    levels_[i] = video_->CreateRenderTarget(w, h,
                                            abstract::TextureFormat::kG11R11B10F);
    std::array<abstract::RenderTarget*, 1> colors = {levels_[i].get()};
    fbos_[i] = video_->CreateRenderTargetGroup(colors, nullptr);
    w /= 2;
    h /= 2;
  }
}

void BloomRenderer::Resize(int width, int height) {
  for (int i = 0; i < kLevelCount; ++i) {
    fbos_[i].reset();
    levels_[i].reset();
  }
  CreateLevels(width, height);
  bloom_texture_ = null_rt_.get();
}

void BloomRenderer::Destroy() {
  for (int i = 0; i < kLevelCount; ++i) {
    fbos_[i].reset();
    levels_[i].reset();
  }
  null_fbo_.reset();
  null_rt_.reset();
  quad_.reset();
  if (downsample_shader_) {
    downsample_shader_->Release();
    downsample_shader_ = nullptr;
  }
  if (upsample_shader_) {
    upsample_shader_->Release();
    upsample_shader_ = nullptr;
  }
  video_ = nullptr;
}

abstract::RenderTarget* BloomRenderer::Render(abstract::RenderTarget* hdr_rt,
                                               float threshold,
                                               float intensity) {
  if (intensity == 0.f) {
    bloom_texture_ = null_rt_.get();
    return null_rt_.get();
  }

  video_->SetDepthTestEnabled(false);
  video_->SetDepthWriteEnabled(false);
  video_->SetBlendEnabled(false);
  video_->SetIndexType(abstract::IndexType::kUInt32);

  // --- Downsample pass --------------------------------------------------------
  // Level 0: threshold extraction from HDR RT → levels_[0] (W/2 × H/2).
  // Subsequent levels: plain box filter (threshold == 0 → no cutoff).
  downsample_shader_->Activate();

  abstract::RenderTarget* src = hdr_rt;
  for (int i = 0; i < kLevelCount; ++i) {
    const float texel_w = 1.f / static_cast<float>(src->GetWidth());
    const float texel_h = 1.f / static_cast<float>(src->GetHeight());

    src->BindAsSampler(0);
    downsample_shader_->SetUniform2f("u_texel_size", texel_w, texel_h);
    // u_threshold = 0 at deeper levels makes max(c - 0, 0) == c (no cutoff).
    downsample_shader_->SetUniformFloat("u_threshold",
                                        (i == 0) ? threshold : 0.f);

    fbos_[i]->BindForWriting();
    video_->SetViewport(0, 0, levels_[i]->GetWidth(), levels_[i]->GetHeight());
    quad_->Set();
    video_->RenderIndexed(quad_->GetNumIndices());
    fbos_[i]->UnbindForWriting();

    video_->UnbindSampler(0);
    src = levels_[i].get();
  }

  // --- Upsample pass ----------------------------------------------------------
  // Accumulate from the smallest level (kLevelCount-1) back up to level 0.
  //
  // At each step i we additive-blend the upsampled levels_[i+1] into levels_[i].
  // Using GPU additive blending avoids a texture feedback loop: only levels_[i+1]
  // is sampled; levels_[i] is written to via the FBO blend state.
  //
  //   levels_[i] = levels_[i]_downsample + tent9(levels_[i+1]) * intensity
  upsample_shader_->Activate();
  upsample_shader_->SetUniformFloat("u_intensity", intensity);
  video_->SetBlendEnabled(true, abstract::BlendFactor::kOne,
                          abstract::BlendFactor::kOne);

  for (int i = kLevelCount - 2; i >= 0; --i) {
    const float texel_w = 1.f / static_cast<float>(levels_[i + 1]->GetWidth());
    const float texel_h = 1.f / static_cast<float>(levels_[i + 1]->GetHeight());

    levels_[i + 1]->BindAsSampler(0);
    upsample_shader_->SetUniform2f("u_texel_size", texel_w, texel_h);

    fbos_[i]->BindForWriting();
    video_->SetViewport(0, 0, levels_[i]->GetWidth(), levels_[i]->GetHeight());
    quad_->Set();
    video_->RenderIndexed(quad_->GetNumIndices());
    fbos_[i]->UnbindForWriting();

    video_->UnbindSampler(0);
  }

  video_->SetBlendEnabled(false);

  bloom_texture_ = levels_[0].get();
  return levels_[0].get();
}

abstract::RenderTarget* BloomRenderer::GetBloomTexture() const {
  return bloom_texture_;
}

}  // namespace renderer
