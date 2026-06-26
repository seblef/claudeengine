#pragma once

#include <memory>

#include "abstract/RenderTarget.h"
#include "abstract/RenderTargetGroup.h"
#include "abstract/VideoDevice.h"
#include "core/Event.h"
#include "core/Vec3f.h"
#include "editor/EditorCameraController.h"
#include "editor/EditorCommandHistory.h"
#include "editor/tools/EditorToolBase.h"
#include "editor/PickingAccelerator.h"
#include "game/GameCamera.h"
#include "game/GameObject.h"

#include <imgui.h>

namespace game {
class MeshTemplate;
}  // namespace game

namespace terrain { class TerrainData; }

namespace editor {

class EditorScene;
class EditorToolbar;
class RenderingSettingsPanel;
class SelectionTool;

// Viewport panel: owns the offscreen render target and camera, and drives the
// scene render each frame.
//
// Each frame (inside ImGui::Begin("Viewport")):
//   1. Detects panel-size changes and recreates the render target + FBO.
//   2. Calls Renderer::Update() to render the scene into the offscreen texture.
//   3. Blits the texture into the panel via ImGui::Image() with a Y-flip
//      (OpenGL FBO origin is bottom-left).
//
// Event routing: the owning EditorWindow calls OnEvent() for every queued
// platform event so the camera controller receives input.
class EditorViewport {
 public:
  explicit EditorViewport(abstract::VideoDevice* video);
  ~EditorViewport();

  // Called from within ImGui::Begin("Viewport"). Checks panel size, runs the
  // render pass, and blits the result via ImGui::Image().
  void Render();

  // Forwards a platform event to the camera controller.
  void OnEvent(const core::Event& event);

  // Sets the scene reference (not owned). Call once after scene creation.
  // Rebuilds the picking accelerator from all current scene objects.
  void SetScene(EditorScene* scene);

  [[nodiscard]] ImVec2                  GetPanelSize()      const { return panel_size_; }
  [[nodiscard]] const game::GameObject* GetSelectedObject() const { return selected_object_; }
  void SetSelectedObject(game::GameObject* obj) { selected_object_ = obj; }

  // Sets the active tool (non-owning). Replaces any previously set tool via
  // OnDeactivate() / OnActivate(). Pass nullptr to fall back to SelectionTool.
  void SetActiveTool(EditorToolBase* tool);

  // Provides the command history used to record placements for undo/redo.
  void SetCommandHistory(EditorCommandHistory* history);

  // Camera state pass-through for map save/restore.
  [[nodiscard]] EditorCameraController::CameraState GetCameraState() const;
  void SetCameraState(const EditorCameraController::CameraState& state);

  // Returns the viewport camera's current world-space transform.
  // Useful for updating the audio listener each frame.
  [[nodiscard]] const core::Mat4f& GetCameraWorldTransform() const;

  // Returns the camera controller's current look-at focus point in world space.
  // Used for placing editor gizmos (e.g. reference gauges) at the scene centre.
  [[nodiscard]] core::Vec3f GetCameraFocusPoint() const;

  // Adjusts the camera focus and distance so the given bounding box fills the view.
  void FrameObject(const core::BBox3& bbox);

  // Provides the snap toolbar so the viewport can read snap state for the grid
  // overlay. Non-owning; must outlive this viewport.
  void SetToolbar(const EditorToolbar* toolbar) { toolbar_ = toolbar; }

  // Enables or disables the terrain wireframe debug overlay.
  static void SetTerrainWireframeDebugEnabled(bool enabled);

  // Updates the picking accelerator for an object whose transform changed
  // outside of the viewport (e.g. from the Properties panel).
  void UpdateMovedObject(game::GameObject* obj);

  // Provides terrain data used to ray-cast placement hit points.
  // Pass nullptr when no terrain is in the scene.
  void SetTerrainData(const terrain::TerrainData* data) { terrain_data_ = data; }
  [[nodiscard]] const terrain::TerrainData* GetTerrainData() const {
    return terrain_data_;
  }

  // Wires the rendering settings panel so physics gizmos can be toggled.
  // Non-owning; must outlive this viewport.
  void SetRenderingSettingsPanel(RenderingSettingsPanel* panel) {
    rendering_settings_panel_ = panel;
  }

  // Returns the viewport's GameCamera (non-owning).
  // Used by PlayModeManager to bind the FPS camera controller.
  [[nodiscard]] game::GameCamera* GetCamera() const { return camera_.get(); }

  // Toggles play mode: when true, the editor orbit camera controller is
  // suspended so the FPS camera can drive the camera unobstructed. Tools,
  // gizmos, and the orbit widget are also hidden.
  void SetInPlayMode(bool playing) { in_play_mode_ = playing; }

 private:
  void ResizeIfNeeded(int w, int h);

  // Immediately places a GameMesh built from tmpl at the floor-plane hit of
  // the ray through mouse_pos, then selects it. Used by the drag-and-drop flow.
  void PlaceMeshAt(ImVec2 mouse_pos, ImVec2 image_pos, ImVec2 image_size,
                   game::MeshTemplate* tmpl);

  // Draws the XZ snap grid overlay when snap is enabled. Projects world-space
  // grid lines into screen space and renders them onto the current ImDrawList.
  void DrawSnapGrid(ImVec2 image_pos, ImVec2 image_size);

  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice*                       video_;

  // Permanent default tool; activated when SetActiveTool(nullptr) is called.
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<SelectionTool>               selection_tool_;

  std::unique_ptr<game::GameCamera>            camera_;
  std::unique_ptr<EditorCameraController>      camera_ctrl_;

  std::unique_ptr<abstract::RenderTarget>      render_target_;
  std::unique_ptr<abstract::RenderTargetGroup> render_fbo_;
  // FBO sharing render_target_ colour with GBuffer depth for depth-tested
  // wireframe rendering.  Recreated alongside render_fbo_ on every resize.
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<abstract::RenderTargetGroup> wireframe_fbo_;

  // cppcheck-suppress unusedStructMember
  EditorScene*      scene_            = nullptr;  // not owned; set by EditorWindow (issue #170)
  // cppcheck-suppress unusedStructMember
  game::GameObject* selected_object_  = nullptr;
  // cppcheck-suppress unusedStructMember
  ImVec2            panel_size_       = {0.f, 0.f};
  // cppcheck-suppress unusedStructMember
  EditorToolBase*   active_tool_base_  = nullptr;  // not owned
  // cppcheck-suppress unusedStructMember
  EditorCommandHistory*   history_           = nullptr;  // not owned; set by EditorWindow

  // cppcheck-suppress unusedStructMember
  PickingAccelerator      picking_acc_;

  // Terrain heightmap for placement hit raycasts; nullptr when no terrain exists.
  // cppcheck-suppress unusedStructMember
  const terrain::TerrainData* terrain_data_ = nullptr;

  // Non-owned; set by EditorWindow so the viewport can read render toggles.
  // cppcheck-suppress unusedStructMember
  RenderingSettingsPanel* rendering_settings_panel_ = nullptr;

  // Non-owned; set by EditorWindow so the viewport can read snap state.
  // cppcheck-suppress unusedStructMember
  const EditorToolbar* toolbar_ = nullptr;

  // True while the editor is in Play mode. Suspends the orbit camera controller
  // and hides editor-only overlays (gizmos, tools, orbit widget).
  // cppcheck-suppress unusedStructMember
  bool in_play_mode_ = false;
};

}  // namespace editor
