#pragma once

#include <array>
#include <memory>
#include <vector>

#include "abstract/ConstantBuffer.h"
#include "abstract/VideoDevice.h"
#include "core/Camera.h"
#include "core/Singleton.h"
#include "renderer/CSMInfos.h"
#include "renderer/GlobalLight.h"
#include "renderer/IVisibilitySystem.h"
#include "renderer/Light.h"
#include "renderer/ShadowMap.h"
#include "renderer/ShadowMapPool.h"

namespace renderer {

// Singleton that owns the shadow depth pass for all shadow-casting lights.
//
// RenderShadowMaps() must be called once per frame after CullAndEnqueue,
// before the geometry pass.  For each shadow-casting spot light it:
//   1. Retrieves the ShadowMap assigned by ShadowMapPool (may be nullptr).
//   2. Computes the light-space view-projection matrix via ComputeShadowVP().
//   3. Culls shadow casters from both visibility systems.
//   4. Renders them into the shadow map's depth-only FBO.
//
// GPU memory is bounded and fixed at startup: all ShadowMaps are pre-allocated
// by ShadowMapPool according to the [shadows] section of config.yaml.
//
// Lifecycle: created by Renderer constructor; destroyed by Renderer destructor.
class ShadowRenderer : public core::Singleton<ShadowRenderer> {
 public:
  explicit ShadowRenderer(abstract::VideoDevice* video);
  ~ShadowRenderer();

  ShadowRenderer(const ShadowRenderer&)            = delete;
  ShadowRenderer& operator=(const ShadowRenderer&) = delete;

  // Assigns pool slots and renders shadow maps for all shadow-casting lights.
  // no_cull: always-visible visibility system (GlobalLight, large actors).
  // octree:  frustum-culled visibility system (most scene objects).
  // camera:  used for screen-space radius computation in pool assignment.
  void RenderShadowMaps(const std::vector<Light*>& lights,
                        const IVisibilitySystem*   no_cull,
                        const IVisibilitySystem*   octree,
                        const core::Camera&        camera);

  // Returns the shadow map assigned to a spot light this frame, or nullptr if none.
  [[nodiscard]] const ShadowMap* GetShadowMap(const Light* light) const;

  // Returns true if a GlobalLight with cast_shadow=true was rendered this frame.
  [[nodiscard]] bool HasCSM() const { return has_csm_; }

  // Returns the filled CSMInfos from the last RenderShadowMaps call.
  // Valid only when HasCSM() == true.
  [[nodiscard]] const CSMInfos& GetCSMInfos() const { return csm_infos_; }

  // Returns the shadow map for cascade index (0–3), or nullptr if !HasCSM().
  [[nodiscard]] const ShadowMap* GetCascadeMap(int index) const;

  // Releases all pool assignments (call on scene clear).
  void ClearShadowMaps();

 private:
  void RenderCascades(const GlobalLight&       light,
                      const IVisibilitySystem* no_cull,
                      const IVisibilitySystem* octree,
                      const core::Camera&      camera);

  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice*                     video_;
  std::unique_ptr<abstract::ConstantBuffer>  shadow_pass_infos_cb_;
  // cppcheck-suppress unusedStructMember
  ShadowMapPool                              pool_;

  // CSM state — populated each frame by RenderShadowMaps when a GlobalLight
  // with cast_shadow=true is present.
  // cppcheck-suppress unusedStructMember
  bool    has_csm_  = false;
  // cppcheck-suppress unusedStructMember
  CSMInfos csm_infos_;
  // cppcheck-suppress unusedStructMember
  std::array<std::unique_ptr<ShadowMap>, kCSMCascadeCount> cascade_maps_;
};

}  // namespace renderer
