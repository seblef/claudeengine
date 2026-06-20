#pragma once

#include <memory>
#include <vector>

#include "abstract/RenderTarget.h"
#include "abstract/RenderTargetGroup.h"
#include "abstract/Shader.h"
#include "abstract/VideoDevice.h"
#include "renderer/GeometryData.h"

namespace renderer {

// Eye adaptation (auto-exposure) renderer using iterative log-luminance downsampling.
//
// Algorithm (runs once per frame before the composite pass):
//   1. Convert the HDR buffer to log-luminance into a half-res R16F target.
//   2. Downsample successively (halving each dimension) until 1×1.
//   3. Read the 1×1 log-luminance pixel back to the CPU.
//   4. Compute target exposure:  target = eye_key / exp(avg_log_lum).
//   5. Smooth toward target:     exposure = mix(prev, target, 1 - exp(-dt * speed)).
//   6. Clamp to [eye_min_exposure, eye_max_exposure] and return for PostProcessInfos::exposure.
//
// On the first frame Update() returns 1.0f immediately (no GPU work) to avoid
// exposing garbage from uninitialised textures.
class EyeAdaptationRenderer {
 public:
  // Allocates the full R16F downsample chain from W/2×H/2 down to 1×1.
  // video must outlive this EyeAdaptationRenderer.
  void Create(abstract::VideoDevice* video, int width, int height);

  // Destroys and recreates all level render targets at the new resolution.
  // Shaders and quad are reused; only the RTs are reallocated.
  void Resize(int width, int height);

  // Releases all GPU resources (RTs, FBOs, shaders, quad).
  void Destroy();

  // Runs the luminance downsample chain and updates current_exposure_.
  //   hdr_rt      : HDR accumulation render target (RGBA16F from EmissiveFBO).
  //   dt          : frame delta time in seconds.
  //   adapt_speed : lerp speed (larger = faster response).
  //   key         : middle-grey key value; target exposure = key / avg_lum.
  //   min_exposure: lower clamp on the output exposure.
  //   max_exposure: upper clamp on the output exposure.
  // Returns the new exposure value to write into PostProcessInfos::exposure.
  // Returns 1.0f on the first frame without rendering any GPU passes.
  float Update(abstract::RenderTarget* hdr_rt, float dt,
               float adapt_speed, float key,
               float min_exposure, float max_exposure);

 private:
  void CreateLevels(int width, int height);

  // Chain of R16F render targets, from level 0 (W/2 × H/2) down to 1×1.
  // cppcheck-suppress unusedStructMember
  std::vector<std::unique_ptr<abstract::RenderTarget>>      levels_;
  // cppcheck-suppress unusedStructMember
  std::vector<std::unique_ptr<abstract::RenderTargetGroup>> fbos_;

  // HDR → log-luminance (first pass into levels_[0]).
  // cppcheck-suppress unusedStructMember
  abstract::Shader* lum_init_shader_       = nullptr;
  // Successive log-luminance halvings (levels_[i-1] → levels_[i]).
  // cppcheck-suppress unusedStructMember
  abstract::Shader* lum_downsample_shader_ = nullptr;

  std::unique_ptr<GeometryData> quad_;

  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice* video_ = nullptr;

  float current_exposure_ = 1.0f;
  bool  first_frame_      = true;
};

}  // namespace renderer
