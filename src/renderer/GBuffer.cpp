#include "renderer/GBuffer.h"

#include <array>

#include "abstract/TextureFormat.h"

namespace renderer {

void GBuffer::Create(abstract::VideoDevice* video, int w, int h) {
  using abstract::TextureFormat;

  albedo_rt_ = video->CreateRenderTarget(w, h, TextureFormat::kRGBA8);
  normal_rt_ = video->CreateRenderTarget(w, h, TextureFormat::kRGBA16F);
  depth_rt_  = video->CreateRenderTarget(w, h, TextureFormat::kDepth24Stencil8);

  std::array<abstract::RenderTarget*, 2> colors = {
      albedo_rt_.get(), normal_rt_.get(),
  };
  fbo_ = video->CreateRenderTargetGroup(colors, depth_rt_.get());
}

void GBuffer::Resize(abstract::VideoDevice* video, int w, int h) {
  fbo_.reset();
  albedo_rt_.reset();
  normal_rt_.reset();
  depth_rt_.reset();
  Create(video, w, h);
}

void GBuffer::BindForWriting() {
  fbo_->BindForWriting();
}

void GBuffer::UnbindForWriting() {
  fbo_->UnbindForWriting();
}

void GBuffer::BindForReading(int first_slot) {
  fbo_->BindForReading(first_slot);
}

abstract::RenderTarget* GBuffer::GetAlbedoRT() const { return albedo_rt_.get(); }
abstract::RenderTarget* GBuffer::GetNormalRT()  const { return normal_rt_.get(); }
abstract::RenderTarget* GBuffer::GetDepthRT()   const { return depth_rt_.get(); }

}  // namespace renderer
