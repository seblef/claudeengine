#pragma once

#include <memory>

#include "abstract/VideoDevice.h"
#include "environment/EnvironmentDesc.h"
#include "environment/WindSystem.h"
#include "environment/WorldTime.h"

namespace editor { class EditorScene; }

namespace editor {

// ImGui panel for editing all environment parameters of the current map.
//
// The panel is docked as a window ("Environment") alongside the Terrain panel.
// When SetContext() is called with a valid scene, all controls are enabled.
// When no scene is set (context is null) the panel shows a "No map loaded"
// message.
//
// Live-preview: every change is applied immediately to the running renderer
// singletons (SkyRenderer, WaterRenderer, CloudRenderer) and to the owned
// WorldTime / WindSystem instances.
//
// Persistence: all changes are written back to the EditorScene's EnvironmentDesc
// via SetEnvironmentDesc(), so the next MapSerializer::Save() round-trips them.
//
// Sky systems (SkyRenderer, WaterRenderer, CloudRenderer) are created on demand
// when their respective enable checkbox is ticked, and reset+destroyed when
// unticked.
class EnvironmentEditorPanel {
 public:
  EnvironmentEditorPanel() = default;
  ~EnvironmentEditorPanel();

  EnvironmentEditorPanel(const EnvironmentEditorPanel&)            = delete;
  EnvironmentEditorPanel& operator=(const EnvironmentEditorPanel&) = delete;

  // Binds the panel to a scene. Pass nullptr to reset to the "no context" state.
  // All renderer subsystems currently owned by the panel are torn down and
  // rebuilt from the scene's EnvironmentDesc when a non-null scene is provided.
  void SetContext(EditorScene* scene, abstract::VideoDevice* video);

  // Advances world time when sky is enabled and time is not paused.
  // Call once per editor frame with the frame delta time.
  void Tick(float dt);

  // Renders all widgets into the current ImGui window.
  // Returns true when any field was edited this frame.
  // Must be called inside an ImGui::Begin / ImGui::End block.
  bool Render();

  // Returns true when a valid scene context is bound.
  [[nodiscard]] bool IsActive() const { return scene_ != nullptr; }

 private:
  // ---- Section render helpers -----------------------------------------------
  bool RenderTimeSection(environment::EnvironmentDesc& env);
  bool RenderSkySection(environment::EnvironmentDesc& env);
  bool RenderCloudSection(environment::EnvironmentDesc& env);
  bool RenderWindSection(environment::EnvironmentDesc& env);
  bool RenderWaterSection(environment::EnvironmentDesc& env);
  bool RenderTreesSection(environment::EnvironmentDesc& env);

  // ---- Subsystem lifecycle --------------------------------------------------
  // Creates SkyRenderer singleton + WorldTime from desc, registers with Renderer.
  void EnableSky(const environment::EnvironmentDesc& desc);
  void DisableSky();

  // Derives global light direction, color and intensity from the current sky
  // state (sun or moon) and pushes the update to the scene's global light.
  // No-op when sky is disabled (world_time_ is null) or no scene is set.
  void UpdateGlobalLightFromSky();

  void EnableWater(const environment::EnvironmentDesc& desc);
  void DisableWater();

  void EnableCloud(const environment::EnvironmentDesc& desc);
  void DisableCloud();

  void EnableWind(const environment::EnvironmentDesc& desc);
  void DisableWind();

  // ---- Context --------------------------------------------------------------
  // cppcheck-suppress unusedStructMember
  EditorScene*           scene_ = nullptr;
  abstract::VideoDevice* video_ = nullptr;

  // ---- Owned runtime objects ------------------------------------------------
  std::unique_ptr<environment::WorldTime>  world_time_;
  std::unique_ptr<environment::WindSystem> wind_system_;

  // ---- Panel UI state -------------------------------------------------------
  bool  time_paused_   = true;
  float wind_angle_deg_ = 0.f;   // cached compass angle for the direction widget
};

}  // namespace editor
