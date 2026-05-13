#include "renderer/ShadowMap.h"

#include <span>

#include "abstract/TextureFormat.h"

namespace renderer {

ShadowMap::ShadowMap(abstract::VideoDevice* video, int resolution)
    : depth_rt_(video->CreateRenderTarget(
          resolution, resolution, abstract::TextureFormat::kDepth32F)),
      fbo_(video->CreateRenderTargetGroup(
          std::span<abstract::RenderTarget*>{}, depth_rt_.get())),
      resolution_(resolution) {}

}  // namespace renderer
