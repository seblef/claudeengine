#pragma once

#include <memory>
#include <vector>

#include "abstract/ConstantBuffer.h"
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
  void Render();

  // Clears the instance queue after drawing.
  void EndRender();

  // Returns the current frame's enqueued lights.
  // Valid after CullAndEnqueue and before EndRender().
  [[nodiscard]] const std::vector<Light*>& GetLights() const { return instances_; }

 private:
  void RenderGlobalLights();
  void RenderLocalLights();

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
};

}  // namespace renderer
