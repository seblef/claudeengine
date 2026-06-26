#include "editor/EditorViewport.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <span>
#include <utility>
#include <vector>

#include "abstract/TextureFormat.h"
#include "core/CoordinateSystem.h"
#include "core/Mat4f.h"
#include "core/ProjectionType.h"
#include "core/Vec3f.h"
#include "core/Vec4f.h"
#include "editor/EditorScene.h"
#include "editor/EditorToolbar.h"
#include "editor/GaugeGizmos.h"
#include "editor/PickingAccelerator.h"
#include "editor/PivotGizmos.h"
#include "editor/PlayerStartGizmos.h"
#include "editor/SoundEmitterGizmos.h"
#include "editor/RenderingSettingsPanel.h"
#include "editor/commands/PlaceObjectCommand.h"
#include "editor/tools/SelectionTool.h"
#include "editor/tools/ViewportRaycast.h"
#include "game/GameMesh.h"
#include "game/GameObjectType.h"
#include "game/GameObject.h"
#include "game/MeshTemplate.h"
#include "physics/PhysicsBody.h"
#include "physics/PhysicsSystem.h"
#include "renderer/Renderer.h"
#include "renderer/WireframeRenderer.h"
#include "terrain/TerrainData.h"

#include <ImGuizmo.h>
#include <imgui.h>
#include <loguru.hpp>

namespace editor {


EditorViewport::~EditorViewport() = default;

EditorViewport::EditorViewport(abstract::VideoDevice* video)
    : video_(video),
      selection_tool_(std::make_unique<SelectionTool>()),
      camera_(std::make_unique<game::GameCamera>(
          core::ProjectionType::kPerspective,
          core::CoordinateSystem::kRightHanded)),
      camera_ctrl_(std::make_unique<EditorCameraController>()) {
  camera_->SetFOV(0.785398f);  // 45 degrees
  camera_->SetMinDepth(0.1f);
  camera_->SetMaxDepth(1000.f);
  camera_ctrl_->SetCamera(camera_.get());
  // SelectionTool is the permanent default base tool; scene and history are
  // null at this point and will be refreshed by SetScene() / SetCommandHistory().
  active_tool_base_ = selection_tool_.get();
}

void EditorViewport::SetScene(EditorScene* scene) {
  scene_ = scene;
  if (scene_) {
    scene_->SetOnObjectAdded([this](game::GameObject* obj) {
      picking_acc_.Add(obj);
    });
    scene_->SetOnObjectRemoved([this](game::GameObject* obj) {
      picking_acc_.Remove(obj);
    });
    picking_acc_.Build(scene_->GetObjects(), scene_->GetBounds());
  }
  // Re-activate the selection tool so it caches the updated scene pointer.
  SetActiveTool(selection_tool_.get());
}

void EditorViewport::OnEvent(const core::Event& event) {
  // In play mode, events are routed to the FPS camera controller by
  // PlayModeManager::OnEvent() — do not also dispatch to editor tools or the
  // orbit camera controller.
  if (in_play_mode_) return;

  const bool keyboard_captured = ImGui::GetIO().WantCaptureKeyboard;

  // Dispatch to the active base tool (e.g. SelectionTool handles Delete key).
  if (active_tool_base_)
    active_tool_base_->OnEvent(event);

  // Always forward key-up events to the camera to clear any held movement
  // flags, but suppress key-down events when ImGui owns the keyboard so that
  // typing in a text field does not accidentally move the camera.
  if (keyboard_captured && event.type == core::EventType::kKeyDown)
    return;

  camera_ctrl_->OnEvent(event);
}

void EditorViewport::ResizeIfNeeded(int w, int h) {
  if (w == static_cast<int>(panel_size_.x) && h == static_cast<int>(panel_size_.y)) return;
  panel_size_ = {static_cast<float>(w), static_cast<float>(h)};
  camera_->SetScreenCenter({panel_size_.x * 0.5f, panel_size_.y * 0.5f});
  renderer::Renderer::Instance().ResizeTargets(w, h);
  render_target_ = video_->CreateRenderTarget(w, h, abstract::TextureFormat::kRGBA8);
  abstract::RenderTarget* color_ptr = render_target_.get();
  render_fbo_ = video_->CreateRenderTargetGroup(
      std::span<abstract::RenderTarget*>(&color_ptr, 1), nullptr);
  // Share the GBuffer depth so light wireframes are occluded by opaque geometry.
  abstract::RenderTarget* depth_ptr =
      renderer::Renderer::Instance().GetGBuffer()->GetDepthRT();
  wireframe_fbo_ = video_->CreateRenderTargetGroup(
      std::span<abstract::RenderTarget*>(&color_ptr, 1), depth_ptr);
}

void EditorViewport::Render() {
  const ImVec2 avail = ImGui::GetContentRegionAvail();
  const int w = std::max(1, static_cast<int>(avail.x));
  const int h = std::max(1, static_cast<int>(avail.y));

  // In play mode the FPS camera controller drives the camera; suspend the orbit
  // camera controller to prevent it from overwriting the FPS camera transform.
  if (!in_play_mode_) {
    // Gate orbit/pan/zoom input: block when the ViewManipulate widget is hovered.
    const bool widget_hovered = ImGuizmo::IsViewManipulateHovered();
    camera_ctrl_->SetViewportHovered(ImGui::IsWindowHovered() && !widget_hovered);
    camera_ctrl_->Update(ImGui::GetIO().DeltaTime);
  }

  ResizeIfNeeded(w, h);

  if (rendering_settings_panel_) {
    renderer::WireframeRenderer::Instance().SetLightGizmosEnabled(
        rendering_settings_panel_->IsOverlayLightsEnabled());
    renderer::WireframeRenderer::Instance().SetParticleGizmosEnabled(
        rendering_settings_panel_->IsOverlayParticlesEnabled());
  }

  renderer::Renderer::Instance().Update(
      static_cast<float>(ImGui::GetTime()),
      camera_->GetCamera(),
      render_fbo_.get());

  // Capture the image top-left before the Image widget advances the cursor.
  const ImVec2 image_pos = ImGui::GetCursorScreenPos();
  camera_ctrl_->SetViewportRect(image_pos.x, image_pos.y, avail.x, avail.y);

  if (render_target_) {
    // uv0=(0,1) uv1=(1,0): Y-flip because OpenGL FBO origin is bottom-left.
    ImGui::Image(render_target_->GetNativeHandle(), avail, ImVec2(0.f, 1.f), ImVec2(1.f, 0.f));
    if (scene_ && ImGui::BeginDragDropTarget()) {
      if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("MESH_TEMPLATE")) {
        auto* tmpl = *static_cast<game::MeshTemplate**>(p->Data);
        PlaceMeshAt(ImGui::GetMousePos(), image_pos, avail, tmpl);
      }
      ImGui::EndDragDropTarget();
    }
  }

  // Delegate picking, gizmo drawing, and bounding-box drawing to the active tool.
  // Skipped in play mode: editor overlays are not relevant during gameplay.
  if (!in_play_mode_ && scene_ && active_tool_base_) {
    EditorToolContext ctx{scene_, camera_.get(), &picking_acc_,
                          history_, video_};
    ctx.terrain_data = terrain_data_;
    active_tool_base_->OnRender(ctx, image_pos, avail);
  }

  if (!in_play_mode_ && rendering_settings_panel_ && scene_
      && physics::PhysicsSystem::IsInstanced()) {
    physics::PhysicsDebugDrawSettings debug_settings;
    debug_settings.drawShapes        =
        rendering_settings_panel_->IsPhysicsDebugShapesEnabled();
    debug_settings.drawConstraints   =
        rendering_settings_panel_->IsPhysicsDebugConstraintsEnabled();
    debug_settings.drawContactPoints =
        rendering_settings_panel_->IsPhysicsDebugContactPointsEnabled();
    debug_settings.drawBroadPhase    =
        rendering_settings_panel_->IsPhysicsDebugBroadPhaseEnabled();

    std::vector<const physics::PhysicsBody*> selected_bodies;
    if (rendering_settings_panel_->GetPhysicsDebugBodyMode() ==
        RenderingSettingsPanel::PhysicsDebugBodyMode::kSelectedOnly) {
      for (game::GameObject* obj : scene_->GetSelection()) {
        if (obj->GetType() == game::GameObjectType::kMesh) {
          const physics::PhysicsBody* body =
              static_cast<game::GameMesh*>(obj)->GetPhysicsBody();
          if (body) selected_bodies.push_back(body);
        }
      }
      debug_settings.selectedBodies = &selected_bodies;
    }

    physics::PhysicsSystem::Instance().DrawDebug(debug_settings);
  }

  // Editor-only overlays: gizmos, highlights, and the orbit widget.
  // Skipped in play mode — the player sees the world without editor decorations.
  if (!in_play_mode_) {
    renderer::WireframeRenderer::Instance().SetHighlightedObject(
        scene_ ? scene_->GetSelectedObject() : nullptr);
    if (scene_) {
      const float cam_dist = camera_ctrl_->GetDistance();
      editor::EnqueueGaugeGizmos(scene_->GetObjects(), scene_->GetSelectedObject());
      editor::EnqueuePivotGizmos(scene_->GetObjects(), scene_->GetSelectedObject());
      if (!rendering_settings_panel_ ||
          rendering_settings_panel_->IsOverlayPlayerStartsEnabled()) {
        editor::EnqueuePlayerStartGizmos(scene_->GetObjects(),
                                         scene_->GetSelectedObject());
      }
      if (!rendering_settings_panel_ ||
          rendering_settings_panel_->IsOverlaySoundsEnabled()) {
        editor::EnqueueSoundEmitterGizmos(scene_->GetObjects(),
                                          scene_->GetSelectedObject(), cam_dist);
      }
    }
    if (wireframe_fbo_ && render_fbo_)
      renderer::WireframeRenderer::Instance().Render(
          *camera_->GetCamera(), wireframe_fbo_.get(), render_fbo_.get());

    DrawSnapGrid(image_pos, avail);

    // XYZ axis overlay — bottom-right corner of the viewport panel.
    // ImGuizmo uses row-major storage with row-vector convention (translation in
    // last row), which is the transpose of our column-vector Mat4f convention.
    const ImVec2 vp_pos  = ImGui::GetWindowPos();
    const ImVec2 vp_size = {static_cast<float>(w), static_cast<float>(h)};
    ImGuizmo::SetRect(vp_pos.x, vp_pos.y, vp_size.x, vp_size.y);
    ImGuizmo::SetDrawlist();

    const core::Mat4f view_t = camera_->GetCamera()->GetViewMatrix().Transpose();
    const core::Mat4f proj_t = camera_->GetCamera()->GetProjectionMatrix().Transpose();
    float view_im[16], proj_im[16];
    std::memcpy(view_im, view_t.Data(), sizeof(view_im));
    std::memcpy(proj_im, proj_t.Data(), sizeof(proj_im));

    // Use the 9-parameter overload so ComputeContext runs first and sets
    // gContext.mProjectionMat. Without it the 5-parameter form reads a zero
    // projection matrix, concludes left-handed, and negates all angles.
    float dummy_model[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    constexpr float kWidgetSize = 88.f;
    const ImVec2 widget_pos = {
        vp_pos.x + vp_size.x - kWidgetSize,
        vp_pos.y + vp_size.y - kWidgetSize,
    };
    ImGuizmo::ViewManipulate(view_im, proj_im, ImGuizmo::TRANSLATE, ImGuizmo::WORLD,
                             dummy_model, camera_ctrl_->GetDistance(),
                             widget_pos, ImVec2(kWidgetSize, kWidgetSize), 0x10101080);

    // ImGuizmo wrote back its row-vector view matrix; transpose to our
    // column-vector convention before updating the camera controller.
    const core::Mat4f view_t_after(view_im);
    if (view_t_after != view_t) {
      camera_ctrl_->SetViewMatrix(view_t_after.Transpose());
    }
  }
}

void EditorViewport::PlaceMeshAt(ImVec2 mouse_pos, ImVec2 image_pos,
                                  ImVec2 image_size, game::MeshTemplate* tmpl) {
  if (!scene_ || !tmpl) return;

  const auto hit = ComputeTerrainHit(camera_.get(), terrain_data_,
                                      mouse_pos, image_pos, image_size);
  if (!hit) return;

  auto mesh = std::make_unique<game::GameMesh>(tmpl);
  mesh->SetWorldTransform(core::Mat4f::Translation(*hit));

  if (history_) {
    history_->Push(std::make_unique<PlaceObjectCommand>(scene_, std::move(mesh)));
  } else {
    game::GameObject* obj = scene_->AddDynamicObject(std::move(mesh));
    scene_->SetSelectedObject(obj);
  }
}

EditorCameraController::CameraState EditorViewport::GetCameraState() const {
  return camera_ctrl_->GetState();
}

core::Vec3f EditorViewport::GetCameraFocusPoint() const {
  return camera_ctrl_->GetState().focus;
}

const core::Mat4f& EditorViewport::GetCameraWorldTransform() const {
  return camera_->GetWorldTransform();
}

void EditorViewport::SetCameraState(const EditorCameraController::CameraState& state) {
  camera_ctrl_->SetState(state);
}

void EditorViewport::SetCommandHistory(EditorCommandHistory* history) {
  history_ = history;
  // Re-activate the current base tool so it picks up the updated history pointer.
  if (active_tool_base_) {
    active_tool_base_->OnDeactivate();
    const EditorToolContext ctx{scene_, camera_.get(), &picking_acc_,
                                history_, video_};
    active_tool_base_->OnActivate(ctx);
  }
}

void EditorViewport::SetTerrainWireframeDebugEnabled(bool enabled) {
  renderer::Renderer::Instance().SetTerrainWireframeEnabled(enabled);
}

void EditorViewport::FrameObject(const core::BBox3& bbox) {
  camera_ctrl_->FrameObject(bbox);
}

void EditorViewport::SetActiveTool(EditorToolBase* tool) {
  if (!tool) tool = selection_tool_.get();
  if (active_tool_base_)
    active_tool_base_->OnDeactivate();
  active_tool_base_ = tool;
  const EditorToolContext ctx{scene_, camera_.get(), &picking_acc_,
                              history_, video_};
  active_tool_base_->OnActivate(ctx);
}

void EditorViewport::UpdateMovedObject(game::GameObject* obj) {
  picking_acc_.UpdateMoved(obj);
}

void EditorViewport::DrawSnapGrid(ImVec2 image_pos, ImVec2 image_size) {
  if (!toolbar_ || !toolbar_->IsSnapEffectivelyEnabled() || !toolbar_->IsShowGrid()) return;

  const float step = toolbar_->GetPositionSnap();
  if (step <= 0.f) return;

  const float cam_dist = camera_ctrl_->GetDistance();

  // Scale the cell count with camera distance so the grid fills the visible
  // viewport regardless of zoom level or snap granularity.
  constexpr int   kMinHalfCells = 10;
  constexpr int   kMaxHalfCells = 200;
  const int half_cells = std::clamp(
      static_cast<int>(cam_dist / step), kMinHalfCells, kMaxHalfCells);

  // Snap the grid origin so every line lands on a world-space multiple of step.
  const core::Vec3f focus = camera_ctrl_->GetState().focus;
  const float cx = std::floor(focus.x / step) * step;
  const float cz = std::floor(focus.z / step) * step;

  // Line endpoints extend far beyond the visible cells (4× camera distance).
  // This keeps the endpoints off-screen so that the one-step shift of cx/cz
  // when the origin snaps is imperceptible (< 0.25% of the total span).
  constexpr float kLineOverextend = 4.f;
  const float line_span = cam_dist * kLineOverextend;

  constexpr int   kMajorEvery  = 10;
  constexpr ImU32 kMinorColor  = IM_COL32(102, 102, 102, 89);
  constexpr ImU32 kMajorColor  = IM_COL32(128, 128, 128, 128);

  const core::Mat4f& vp = camera_->GetCamera()->GetViewProjectionMatrix();
  ImDrawList* dl = ImGui::GetWindowDrawList();

  // Projects a clip-space point to screen coordinates.
  auto to_screen = [&](const core::Vec4f& c) -> ImVec2 {
    const float inv_w = 1.f / c.w;
    return {(c.x * inv_w + 1.f) * 0.5f * image_size.x + image_pos.x,
            (1.f - c.y * inv_w) * 0.5f * image_size.y + image_pos.y};
  };

  // Draws a world-space XZ line from (x0,z0) to (x1,z1) with near-plane clipping.
  // When one endpoint is behind the camera (w <= 0), the line is clipped to the
  // w = 0 boundary so both axis directions remain visible regardless of yaw.
  constexpr float kWMin = 1e-4f;
  auto draw_line = [&](float x0, float z0, float x1, float z1, ImU32 color) {
    core::Vec4f a = core::Vec4f(x0, 0.f, z0, 1.f) * vp;
    core::Vec4f b = core::Vec4f(x1, 0.f, z1, 1.f) * vp;
    if (a.w < kWMin && b.w < kWMin) return;
    if (a.w < kWMin) {
      const float t = (kWMin - a.w) / (b.w - a.w);
      a = a + (b - a) * t;
    } else if (b.w < kWMin) {
      const float t = (kWMin - b.w) / (a.w - b.w);
      b = b + (a - b) * t;
    }
    dl->AddLine(to_screen(a), to_screen(b), color, 1.f);
  };

  for (int i = -half_cells; i <= half_cells; ++i) {
    const ImU32 color = (i % kMajorEvery == 0) ? kMajorColor : kMinorColor;
    const float fi = static_cast<float>(i);

    // Horizontal line (constant Z, spans X)
    draw_line(cx - line_span, cz + fi * step, cx + line_span, cz + fi * step, color);

    // Vertical line (constant X, spans Z)
    draw_line(cx + fi * step, cz - line_span, cx + fi * step, cz + line_span, color);
  }
}

}  // namespace editor
