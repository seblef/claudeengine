#pragma once

#include <memory>

#include "abstract/RenderTarget.h"
#include "abstract/RenderTargetGroup.h"
#include "abstract/VideoDevice.h"
#include "core/Event.h"
#include "editor/EditorCameraController.h"
#include "game/GameCamera.h"

#include <imgui.h>

namespace game {
class GameObject;
}

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

 private:
  void ResizeIfNeeded(int w, int h);

  // Casts a world-space ray from mouse_pos and selects the nearest hit object.
  void PickObjectAt(ImVec2 mouse_pos, ImVec2 image_pos, ImVec2 image_size);

  // Draws the selected object's world bounding box as 12 orange wireframe edges.
  void DrawSelectedBBox(ImDrawList* dl, ImVec2 image_pos, ImVec2 image_size) const;

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
  bool              selection_active_ = true;
};

}  // namespace editor
