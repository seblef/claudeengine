#include "renderer/EmissiveFBO.h"

#include <array>

#include "abstract/TextureFormat.h"

namespace renderer {

void EmissiveFBO::Create(abstract::VideoDevice* video, int w, int h,
                         abstract::RenderTarget* depth_rt) {
  hdr_rt_ = video->CreateRenderTarget(w, h, abstract::TextureFormat::kRGBA16F);

  std::array<abstract::RenderTarget*, 1> colors = {hdr_rt_.get()};
  fbo_ = video->CreateRenderTargetGroup(colors, depth_rt);
}

void EmissiveFBO::Resize(abstract::VideoDevice* video, int w, int h,
                         abstract::RenderTarget* depth_rt) {
  fbo_.reset();
  hdr_rt_.reset();
  Create(video, w, h, depth_rt);
}

void EmissiveFBO::BindForWriting() {
  fbo_->BindForWriting();
}

void EmissiveFBO::UnbindForWriting() {
  fbo_->UnbindForWriting();
}

abstract::RenderTarget* EmissiveFBO::GetHDRRT() const {
  return hdr_rt_.get();
}

abstract::RenderTargetGroup* EmissiveFBO::GetRenderTargetGroup() const {
  return fbo_.get();
}

}  // namespace renderer
