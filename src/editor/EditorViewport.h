#pragma once

#include <functional>
#include <memory>

#include "abstract/RenderTarget.h"
#include "abstract/RenderTargetGroup.h"
#include "abstract/VideoDevice.h"
#include "core/Event.h"
#include "editor/EditorCameraController.h"
#include "editor/EditorTool.h"
#include "game/GameCamera.h"

#include <imgui.h>

namespace game {
class GameObject;
class MeshTemplate;
}  // namespace game

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
  void SetScene(EditorScene* scene) { scene_ = scene; }

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

  // Called after a mesh is placed to restore the selection tool.
  // Set by EditorWindow so EditorViewport can notify it without a back-pointer.
  void SetOnPlacementDone(std::function<void()> cb) { on_placement_done_ = std::move(cb); }

 private:
  void ResizeIfNeeded(int w, int h);

  // Casts a world-space ray from mouse_pos and selects the nearest hit object.
  void PickObjectAt(ImVec2 mouse_pos, ImVec2 image_pos, ImVec2 image_size);

  // Places a GameMesh from pending_mesh_template_ at the y=0 floor-plane
  // intersection of the ray through mouse_pos, then clears the pending template.
  void PlaceMesh(ImVec2 mouse_pos, ImVec2 image_pos, ImVec2 image_size);

  // Draws the selected object's world bounding box as 12 orange wireframe edges.
  void DrawSelectedBBox(ImDrawList* dl, ImVec2 image_pos, ImVec2 image_size) const;

  // Draws wireframe overlays for all lights in the scene onto dl.
  // Selected lights are drawn in orange; others in yellow.
  void DrawLightsOverlay(ImDrawList* dl, ImVec2 image_pos, ImVec2 image_size) const;

  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice*                       video_;

  std::unique_ptr<game::GameCamera>            camera_;
  std::unique_ptr<EditorCameraController>      camera_ctrl_;

  std::unique_ptr<abstract::RenderTarget>      render_target_;
  std::unique_ptr<abstract::RenderTargetGroup> render_fbo_;

  // cppcheck-suppress unusedStructMember
  EditorScene*      scene_            = nullptr;  // not owned; set by EditorWindow (issue #170)
  // cppcheck-suppress unusedStructMember
  game::GameObject* selected_object_  = nullptr;
  // cppcheck-suppress unusedStructMember
  ImVec2            panel_size_       = {0.f, 0.f};
  // cppcheck-suppress unusedStructMember
  bool              selection_active_        = true;
  // cppcheck-suppress unusedStructMember
  EditorTool        active_tool_             = EditorTool::kSelection;
  // Non-null while the user is in click-to-place mode for a mesh.
  // cppcheck-suppress unusedStructMember
  game::MeshTemplate* pending_mesh_template_ = nullptr;
  // cppcheck-suppress unusedStructMember
  std::function<void()> on_placement_done_;
};

}  // namespace editor
