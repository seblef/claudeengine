#pragma once

#include <array>
#include <memory>

#include "abstract/RenderTarget.h"
#include "abstract/RenderTargetGroup.h"
#include "abstract/Shader.h"
#include "abstract/VideoDevice.h"
#include "renderer/GeometryData.h"

namespace renderer {

// Bloom post-process pass using a mip-chain downsample / upsample approach.
//
// Algorithm (popularised by Epic / Call of Duty Advanced Warfare):
//   1. Downsample: bright-extract the HDR buffer into kLevelCount half-size
//      mip levels, applying a luminance threshold at level 0 only.
//   2. Upsample: accumulate from the smallest level back to full resolution,
//      additively blending each level into the one above.
//   3. The composite shader blends the result over the original HDR buffer.
//
// All levels use R11F_G11F_B10F (lower bandwidth than RGBA16F; no alpha needed).
// Level sizes: [W/2, W/4, W/8, W/16, W/32, W/64] × corresponding heights.
//
// When Render() is called with intensity == 0 the pass is a no-op and a
// pre-allocated 1×1 black render target is returned.
class BloomRenderer {
 public:
  // Allocates kLevelCount R11F_G11F_B10F render targets plus a 1×1 black
  // no-op target. Shaders and the fullscreen quad are also created here.
  // video must outlive this BloomRenderer.
  void Create(abstract::VideoDevice* video, int width, int height);

  // Destroys and recreates all level render targets at the new resolution.
  // Shaders and quad are reused; only the RTs are reallocated.
  void Resize(int width, int height);

  // Releases all GPU resources (RTs, FBOs, shaders, quad).
  void Destroy();

  // Runs the full downsample→upsample chain.
  //   hdr_rt    : HDR accumulation render target (RGBA16F from EmissiveFBO).
  //   threshold : luminance cutoff applied only at downsample level 0.
  //   intensity : per-level blend weight in the upsample pass.
  // Returns the full-resolution bloom render target (levels_[0]).
  // Returns the 1×1 black no-op target immediately when intensity == 0.
  abstract::RenderTarget* Render(abstract::RenderTarget* hdr_rt,
                                 float threshold, float intensity);

  // Returns the most recently computed bloom render target.
  // Valid after the first Render() call; undefined before Create().
  [[nodiscard]] abstract::RenderTarget* GetBloomTexture() const;

 private:
  static constexpr int kLevelCount = 6;

  // Creates kLevelCount R11F_G11F_B10F RTs and their single-attachment FBOs.
  void CreateLevels(int width, int height);

  // One R11F_G11F_B10F render target per mip level (half-res at each step).
  // cppcheck-suppress unusedStructMember
  std::array<std::unique_ptr<abstract::RenderTarget>,      kLevelCount> levels_;
  // cppcheck-suppress unusedStructMember
  std::array<std::unique_ptr<abstract::RenderTargetGroup>, kLevelCount> fbos_;

  // 1×1 black render target returned when intensity == 0 (no-op path).
  std::unique_ptr<abstract::RenderTarget>      null_rt_;
  std::unique_ptr<abstract::RenderTargetGroup> null_fbo_;

  // cppcheck-suppress unusedStructMember
  abstract::Shader* downsample_shader_ = nullptr;
  // cppcheck-suppress unusedStructMember
  abstract::Shader* upsample_shader_   = nullptr;

  std::unique_ptr<GeometryData> quad_;

  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice* video_ = nullptr;

  // Tracks which RT was last written so GetBloomTexture() is well-defined.
  abstract::RenderTarget* bloom_texture_ = nullptr;
};

}  // namespace renderer
