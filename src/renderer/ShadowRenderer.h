#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include "abstract/ConstantBuffer.h"
#include "abstract/Shader.h"
#include "abstract/VideoDevice.h"
#include "core/Mat4f.h"
#include "core/Singleton.h"
#include "renderer/IVisibilitySystem.h"
#include "renderer/Light.h"
#include "renderer/ShadowMap.h"

namespace renderer {

// Default shadow map resolution when none is specified per-light.
constexpr int kDefaultShadowResolution = 1024;

// Singleton that owns the shadow depth pass for all shadow-casting lights.
//
// RenderShadowMaps() must be called once per frame after CullAndEnqueue,
// before the geometry pass.  For each shadow-casting spot light it:
//   1. Allocates (or reuses) a ShadowMap.
//   2. Computes the light-space view-projection matrix.
//   3. Culls shadow casters from both visibility systems.
//   4. Renders them into the shadow map's depth-only FBO.
//
// GlobalLight (CSM) and OmniLight (cube maps) are deferred to later issues.
//
// Lifecycle: created by Renderer constructor; destroyed by Renderer destructor.
class ShadowRenderer : public core::Singleton<ShadowRenderer> {
 public:
  explicit ShadowRenderer(abstract::VideoDevice* video);
  ~ShadowRenderer();

  ShadowRenderer(const ShadowRenderer&)            = delete;
  ShadowRenderer& operator=(const ShadowRenderer&) = delete;

  // Renders shadow maps for all shadow-casting lights visible this frame.
  // no_cull: always-visible visibility system (GlobalLight, large actors).
  // octree:  frustum-culled visibility system (most scene objects).
  void RenderShadowMaps(const std::vector<Light*>& lights,
                        const IVisibilitySystem* no_cull,
                        const IVisibilitySystem* octree);

  // Returns the shadow map assigned to a light, or nullptr if none exists.
  [[nodiscard]] const ShadowMap* GetShadowMap(const Light* light) const;

  // Releases all allocated shadow maps (call on scene clear).
  void ClearShadowMaps();

 private:
  // Computes the light-space VP matrix for a spot light (circle or rectangle).
  [[nodiscard]] static core::Mat4f ComputeLightVP(const Light* light);

  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice* video_;
  // cppcheck-suppress unusedStructMember
  abstract::Shader* depth_shader_;
  std::unique_ptr<abstract::ConstantBuffer> shadow_pass_infos_cb_;

  // cppcheck-suppress unusedStructMember
  std::unordered_map<const Light*, std::unique_ptr<ShadowMap>> shadow_maps_;
};

}  // namespace renderer
