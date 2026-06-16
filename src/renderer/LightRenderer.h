#pragma once

#include <memory>
#include <vector>

#include "abstract/ConstantBuffer.h"
#include "abstract/RenderTarget.h"
#include "abstract/Shader.h"
#include "abstract/VideoDevice.h"
#include "core/Singleton.h"
#include "renderer/GeometryData.h"
#include "renderer/Light.h"

namespace renderer {

// Singleton batch renderer for Light instances.
//
// Holds four pre-built volume meshes (unit quad, unit sphere, unit cone,
// unit pyramid) and four shaders (one per LightType, each bundling its own
// VS and PS).  Instances are sorted by type before rendering so shader and
// geometry switches happen as rarely as possible.
//
// Rendering strategy:
//   GlobalLight:
//     Additive blend, depth write OFF, stencil OFF.
//     One fullscreen-quad draw per instance.
//   OmniLight / CircleSpotLight / RectangleSpotLight:
//     Two sub-passes per light:
//       A (stencil fill): color off, no culling, stencil always; dpfail
//          front→INCR_WRAP, back→DECR_WRAP.
//       B (lighting):     color on, back culling, stencil NOT_EQUAL 0,
//          additive blend.
//
// Lifecycle: created by Renderer constructor; destroyed by Renderer destructor.
class LightRenderer : public core::Singleton<LightRenderer> {
 public:
  explicit LightRenderer(abstract::VideoDevice* video);
  ~LightRenderer();

  LightRenderer(const LightRenderer&)            = delete;
  LightRenderer& operator=(const LightRenderer&) = delete;

  // Enqueues a light for rendering in the current frame.
  void AddLight(Light* light);

  // Sorts instances by type and issues all draw calls into the currently
  // bound HDR render target.  Caller must have bound G-buffer samplers.
  // When disable_shadows is true, cast_shadow is forced to 0 for every light,
  // bypassing shadow-map lookups entirely (used by preview renders).
  void Render(bool disable_shadows = false);

  // Refills and binds light_infos_cb_ with the first GlobalLight so that
  // forward-pass shaders (e.g. foliage billboards) can sample the correct
  // directional light data after local lights have overwritten the buffer.
  // No-op when no GlobalLight is enqueued.
  void BindGlobalLight();

  // Provides a cloud shadow texture to be bound at sampler 13 and its uniforms
  // set on the global_light shader during RenderGlobalLights().
  // Passing nullptr disables cloud shadow for the current frame.
  void SetCloudShadow(abstract::RenderTarget* texture,
                      float coverage_radius, float intensity);

  // Clears the instance queue after drawing.
  void EndRender();

  // Returns the current frame's enqueued lights.
  // Valid after CullAndEnqueue and before EndRender().
  [[nodiscard]] const std::vector<Light*>& GetLights() const { return instances_; }

 private:
  void RenderGlobalLights(bool disable_shadows);
  void RenderLocalLights(bool disable_shadows);

  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice* video_;

  // One shader per light type (each bundles its own VS + PS).
  // cppcheck-suppress unusedStructMember
  abstract::Shader* global_shader_;
  // cppcheck-suppress unusedStructMember
  abstract::Shader* omni_shader_;
  // cppcheck-suppress unusedStructMember
  abstract::Shader* circle_spot_shader_;
  // cppcheck-suppress unusedStructMember
  abstract::Shader* rect_spot_shader_;

  // Per-light constant buffer bound to UBO slot 4.
  std::unique_ptr<abstract::ConstantBuffer> light_infos_cb_;

  // Volume meshes, created once at construction.
  std::unique_ptr<GeometryData> quad_;     // GlobalLight — fullscreen quad
  std::unique_ptr<GeometryData> sphere_;   // OmniLight   — unit UV sphere
  std::unique_ptr<GeometryData> cone_;     // CircleSpot  — unit cone (45° half-angle)
  std::unique_ptr<GeometryData> pyramid_;  // RectSpot    — unit pyramid

  // cppcheck-suppress unusedStructMember
  std::vector<Light*> instances_;

  // Cloud shadow: set each frame before Render(); nullptr disables the feature.
  // cppcheck-suppress unusedStructMember
  abstract::RenderTarget* cloud_shadow_rt_       = nullptr;
  // cppcheck-suppress unusedStructMember
  float                   cloud_shadow_coverage_ = 4000.f;
  // cppcheck-suppress unusedStructMember
  float                   cloud_shadow_intensity_ = 0.f;
};

}  // namespace renderer
