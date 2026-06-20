#include "renderer/EyeAdaptationRenderer.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "abstract/TextureFormat.h"
#include "renderer/GeometryUtils.h"

namespace renderer {

void EyeAdaptationRenderer::Create(abstract::VideoDevice* video,
                                    int width, int height) {
  video_                 = video;
  lum_init_shader_       = video_->CreateShader("eye_adaptation/lum_init");
  lum_downsample_shader_ = video_->CreateShader("eye_adaptation/lum_downsample");
  quad_                  = CreateQuad(video_);
  CreateLevels(width, height);
}

void EyeAdaptationRenderer::CreateLevels(int width, int height) {
  int w = width  / 2;
  int h = height / 2;
  while (true) {
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    levels_.push_back(
        video_->CreateRenderTarget(w, h, abstract::TextureFormat::kR16F));
    std::array<abstract::RenderTarget*, 1> colors = {levels_.back().get()};
    fbos_.push_back(video_->CreateRenderTargetGroup(colors, nullptr));
    if (w == 1 && h == 1) break;
    w /= 2;
    h /= 2;
  }
}

void EyeAdaptationRenderer::Resize(int width, int height) {
  fbos_.clear();
  levels_.clear();
  CreateLevels(width, height);
}

void EyeAdaptationRenderer::Destroy() {
  fbos_.clear();
  levels_.clear();
  quad_.reset();
  if (lum_init_shader_) {
    lum_init_shader_->Release();
    lum_init_shader_ = nullptr;
  }
  if (lum_downsample_shader_) {
    lum_downsample_shader_->Release();
    lum_downsample_shader_ = nullptr;
  }
  video_ = nullptr;
}

float EyeAdaptationRenderer::Update(abstract::RenderTarget* hdr_rt,
                                     float dt, float adapt_speed,
                                     float key, float min_exposure,
                                     float max_exposure) {
  if (first_frame_) {
    first_frame_ = false;
    return current_exposure_;
  }

  video_->SetDepthTestEnabled(false);
  video_->SetDepthWriteEnabled(false);
  video_->SetBlendEnabled(false);
  video_->SetIndexType(abstract::IndexType::kUInt32);

  // Pass 0: HDR → log-luminance (levels_[0], W/2 × H/2).
  lum_init_shader_->Activate();
  hdr_rt->BindAsSampler(0);
  fbos_[0]->BindForWriting();
  video_->SetViewport(0, 0, levels_[0]->GetWidth(), levels_[0]->GetHeight());
  quad_->Set();
  video_->RenderIndexed(quad_->GetNumIndices());
  fbos_[0]->UnbindForWriting();
  video_->UnbindSampler(0);

  // Successive halvings until 1×1.
  lum_downsample_shader_->Activate();
  for (int i = 1; i < static_cast<int>(levels_.size()); ++i) {
    const float texel_w = 1.f / static_cast<float>(levels_[i - 1]->GetWidth());
    const float texel_h = 1.f / static_cast<float>(levels_[i - 1]->GetHeight());
    levels_[i - 1]->BindAsSampler(0);
    lum_downsample_shader_->SetUniform2f("u_texel_size", texel_w, texel_h);
    fbos_[i]->BindForWriting();
    video_->SetViewport(0, 0, levels_[i]->GetWidth(), levels_[i]->GetHeight());
    quad_->Set();
    video_->RenderIndexed(quad_->GetNumIndices());
    fbos_[i]->UnbindForWriting();
    video_->UnbindSampler(0);
  }

  // CPU readback from the 1×1 target; compute and smooth exposure.
  const float avg_log_lum = video_->ReadPixelF(fbos_.back().get(), 0, 0);
  const float avg_lum     = std::exp(avg_log_lum);
  const float target_exp  = key / avg_lum;
  const float t           = 1.f - std::exp(-dt * adapt_speed);
  current_exposure_ = std::lerp(current_exposure_, target_exp, t);
  current_exposure_ = std::clamp(current_exposure_, min_exposure, max_exposure);
  return current_exposure_;
}

}  // namespace renderer
