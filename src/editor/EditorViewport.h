#pragma once

#include <functional>
#include <memory>
#include <optional>

#include "abstract/RenderTarget.h"
#include "abstract/RenderTargetGroup.h"
#include "abstract/VideoDevice.h"
#include "core/Event.h"
#include "core/Vec3f.h"
#include "editor/EditorCameraController.h"
#include "editor/EditorCommandHistory.h"
#include "editor/EditorTool.h"
#include "editor/LightWireframeRenderer.h"
#include "editor/ParticleGizmoRenderer.h"
#include "editor/PlayerStartGizmoRenderer.h"
#include "editor/PickingAccelerator.h"
#include "game/GameCamera.h"
#include "game/GameObject.h"
#include "renderer/Light.h"

#include <imgui.h>

namespace game {
class MeshTemplate;
}  // namespace game

namespace terrain { class TerrainData; }

namespace editor {

class EditorScene;

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
  ~EditorViewport() = default;

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

  // Enables or disables object picking on LMB release. Driven by the toolbar
  // selection tool state (issue #174).
  void SetSelectionActive(bool active) { selection_active_ = active; }

  // Sets the active tool, controlling which ImGuizmo operation is shown.
  void SetActiveTool(EditorTool tool) { active_tool_ = tool; }

  // Enters mesh placement mode: the next LMB click places a GameMesh built
  // from tmpl at the y=0 floor-plane hit, then clears the pending template.
  void SetPendingMeshTemplate(game::MeshTemplate* tmpl);

  // Enters light placement mode: a preview light of the given type follows the
  // cursor at y=10 until the user clicks (LMB) to confirm placement.
  // Pass std::nullopt to cancel and remove the preview from the scene.
  void SetPendingLightType(std::optional<renderer::LightType> type);

  // Enters player-start placement mode: a GamePlayerStart preview follows the
  // cursor until the user clicks (LMB) to confirm placement.
  void SetPendingPlayerStart();

  // Enters particle-system placement mode: a GameParticleSystem built from the
  // named template follows the cursor until the user clicks (LMB) to confirm.
  void SetPendingParticleSystem(const std::string& template_name);

  // Called after a mesh is placed to restore the selection tool.
  // Set by EditorWindow so EditorViewport can notify it without a back-pointer.
  void SetOnPlacementDone(std::function<void()> cb) { on_placement_done_ = std::move(cb); }

  // Provides the command history used to record placements for undo/redo.
  void SetCommandHistory(EditorCommandHistory* history) { history_ = history; }

  // Camera state pass-through for map save/restore.
  [[nodiscard]] EditorCameraController::CameraState GetCameraState() const;
  void SetCameraState(const EditorCameraController::CameraState& state);

  // Enables or disables the terrain wireframe debug overlay.
  void SetTerrainWireframeDebugEnabled(bool enabled) {
    terrain_wireframe_debug_ = enabled;
  }

  // Provides terrain data used to ray-cast the sculpt brush hit point.
  // Pass nullptr when no terrain is in the scene.
  void SetTerrainData(const terrain::TerrainData* data) { terrain_data_ = data; }

  // Enables or disables terrain sculpt mode. When active, LMB drag fires the
  // sculpt callbacks instead of performing object selection.
  void SetSculptActive(bool active) { sculpt_active_ = active; }

  // Callback fired each frame while LMB is held in sculpt mode, with the
  // terrain world XZ hit (wx, wz), a first_touch flag, and the frame delta dt.
  void SetOnSculptBrush(std::function<void(float, float, bool, float)> cb) {
    on_sculpt_brush_ = std::move(cb);
  }

  // Callback fired when the sculpt drag stroke ends (LMB released).
  void SetOnSculptEnd(std::function<void()> cb) {
    on_sculpt_end_ = std::move(cb);
  }

 private:
  void ResizeIfNeeded(int w, int h);

  // Casts a world-space ray from mouse_pos and selects the nearest hit object.
  void PickObjectAt(ImVec2 mouse_pos, ImVec2 image_pos, ImVec2 image_size);

  // Returns the world-space terrain hit point for a ray through mouse_pos, or
  // std::nullopt if the ray misses the terrain bounds.
  [[nodiscard]] std::optional<core::Vec3f>
  ComputeTerrainHit(ImVec2 mouse_pos, ImVec2 image_pos, ImVec2 image_size) const;

  // Enters preview mode: obj becomes the pending preview, height sets its Y
  // position above the floor-plane hit, cursor is shown while hovering.
  void BeginPreview(std::unique_ptr<game::GameObject> obj,
                    float height, ImGuiMouseCursor cursor);

  // Cancels preview: removes the preview object from the scene if present.
  void CancelPreview();

  // Moves the preview to the y=0 floor-plane hit + preview_height_. Transfers
  // it into the scene on the first valid hit.
  void UpdatePreviewPosition(ImVec2 mouse_pos, ImVec2 image_pos, ImVec2 image_size);

  // Finalises placement: selects the preview object in place and exits placement mode.
  void PlacePreview();

  // Immediately places a GameMesh built from tmpl at the y=0 floor-plane hit of
  // the ray through mouse_pos, then selects it. Used by the drag-and-drop flow.
  void PlaceMeshAt(ImVec2 mouse_pos, ImVec2 image_pos, ImVec2 image_size,
                   game::MeshTemplate* tmpl);

  // Draws the selected object's world bounding box as 12 orange wireframe edges.
  void DrawSelectedBBox(ImDrawList* dl, ImVec2 image_pos, ImVec2 image_size) const;

  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice*                       video_;

  std::unique_ptr<game::GameCamera>            camera_;
  std::unique_ptr<EditorCameraController>      camera_ctrl_;

  std::unique_ptr<abstract::RenderTarget>      render_target_;
  std::unique_ptr<abstract::RenderTargetGroup> render_fbo_;
  // FBO sharing render_target_ colour with GBuffer depth for depth-tested
  // wireframe rendering.  Recreated alongside render_fbo_ on every resize.
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<abstract::RenderTargetGroup> wireframe_fbo_;

  // cppcheck-suppress unusedStructMember
  LightWireframeRenderer                       light_wireframe_;
  // cppcheck-suppress unusedStructMember
  PlayerStartGizmoRenderer                     player_start_gizmo_;
  // cppcheck-suppress unusedStructMember
  ParticleGizmoRenderer                        particle_gizmo_;

  // cppcheck-suppress unusedStructMember
  EditorScene*      scene_            = nullptr;  // not owned; set by EditorWindow (issue #170)
  // cppcheck-suppress unusedStructMember
  game::GameObject* selected_object_  = nullptr;
  // cppcheck-suppress unusedStructMember
  ImVec2            panel_size_       = {0.f, 0.f};
  // cppcheck-suppress unusedStructMember
  bool              selection_active_  = true;
  // cppcheck-suppress unusedStructMember
  EditorTool        active_tool_       = EditorTool::kSelection;
  // True while any object-creation preview is active.
  // cppcheck-suppress unusedStructMember
  bool              preview_active_    = false;
  // Held locally until the first valid floor hit, then transferred to the scene.
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<game::GameObject>   pending_preview_;
  // Non-owning pointer to the preview once it lives in the scene.
  // cppcheck-suppress unusedStructMember
  game::GameObject*                   preview_object_  = nullptr;
  // Y position of the preview above the floor-plane hit point.
  // cppcheck-suppress unusedStructMember
  float                               preview_height_  = 0.f;
  // Cursor shown while hovering the viewport in placement mode.
  // cppcheck-suppress unusedStructMember
  ImGuiMouseCursor                    preview_cursor_  = ImGuiMouseCursor_None;
  // cppcheck-suppress unusedStructMember
  std::function<void()>   on_placement_done_;
  // cppcheck-suppress unusedStructMember
  EditorCommandHistory*   history_           = nullptr;  // not owned; set by EditorWindow

  // cppcheck-suppress unusedStructMember
  PickingAccelerator      picking_acc_;

  // cppcheck-suppress unusedStructMember
  bool terrain_wireframe_debug_ = false;

  // Sculpt brush state.
  // cppcheck-suppress unusedStructMember
  const terrain::TerrainData* terrain_data_        = nullptr;
  // cppcheck-suppress unusedStructMember
  bool                        sculpt_active_        = false;
  // cppcheck-suppress unusedStructMember
  bool                        sculpt_stroke_active_ = false;
  // cppcheck-suppress unusedStructMember
  std::function<void(float, float, bool, float)> on_sculpt_brush_;
  // cppcheck-suppress unusedStructMember
  std::function<void()>                          on_sculpt_end_;

  // Gizmo drag-state tracking for TransformCommand (issue #236).
  // cppcheck-suppress unusedStructMember
  bool                    gizmo_was_using_          = false;
  // cppcheck-suppress unusedStructMember
  core::Mat4f             gizmo_before_transform_;
};

}  // namespace editor
