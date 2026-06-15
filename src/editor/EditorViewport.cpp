#include "editor/EditorViewport.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <optional>
#include <span>
#include <utility>

#include "abstract/TextureFormat.h"
#include "core/CoordinateSystem.h"
#include "terrain/TerrainData.h"
#include "core/Mat4f.h"
#include "core/ProjectionType.h"
#include "core/Vec3f.h"
#include "editor/EditorScene.h"
#include "editor/LightWireframeRenderer.h"
#include "editor/PickingAccelerator.h"
#include "editor/commands/PlaceObjectCommand.h"
#include "game/GameLight.h"
#include "game/GameMesh.h"
#include "game/GameObject.h"
#include "game/GameParticleSystem.h"
#include "game/GamePlayerStart.h"
#include "game/MeshTemplate.h"
#include "particles/ParticleSystemTemplate.h"
#include "renderer/Light.h"
#include "renderer/Renderer.h"

#include <ImGuizmo.h>
#include <imgui.h>
#include <loguru.hpp>

namespace editor {

EditorViewport::EditorViewport(abstract::VideoDevice* video)
    : video_(video),
      camera_(std::make_unique<game::GameCamera>(
          core::ProjectionType::kPerspective,
          core::CoordinateSystem::kRightHanded)),
      camera_ctrl_(std::make_unique<EditorCameraController>()),
      light_wireframe_(video),
      player_start_gizmo_(video),
      particle_gizmo_(video) {
  camera_->SetFOV(0.785398f);  // 45 degrees
  camera_->SetMinDepth(0.1f);
  camera_->SetMaxDepth(1000.f);
  camera_ctrl_->SetCamera(camera_.get());
  // SelectionTool is the permanent default base tool; scene and history are
  // null at this point and will be refreshed by SetScene() / SetCommandHistory().
  active_tool_base_ = &selection_tool_;
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
  SetActiveTool(&selection_tool_);
}

void EditorViewport::OnEvent(const core::Event& event) {
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

void EditorViewport::BeginPreview(std::unique_ptr<game::GameObject> obj,
                                   float height, ImGuiMouseCursor cursor) {
  pending_preview_ = std::move(obj);
  preview_height_  = height;
  preview_cursor_  = cursor;
  preview_active_  = true;
  SetSelectionActive(false);
}

void EditorViewport::CancelPreview() {
  if (preview_object_ && scene_) {
    scene_->RemoveDynamicObject(preview_object_);
    preview_object_ = nullptr;
  }
  pending_preview_.reset();
  preview_active_ = false;
  SetSelectionActive(true);
}

void EditorViewport::SetPendingMeshTemplate(game::MeshTemplate* tmpl) {
  if (tmpl)
    BeginPreview(std::make_unique<game::GameMesh>(tmpl), 0.f, ImGuiMouseCursor_None);
  else
    CancelPreview();
}

void EditorViewport::SetPendingLightType(std::optional<renderer::LightType> type) {
  if (type.has_value())
    BeginPreview(std::make_unique<game::GameLight>(*type), 10.f,
                 ImGuiMouseCursor_ResizeAll);
  else
    CancelPreview();
}

void EditorViewport::SetPendingPlayerStart() {
  BeginPreview(std::make_unique<game::GamePlayerStart>(), 0.f,
               ImGuiMouseCursor_ResizeAll);
}

void EditorViewport::SetPendingParticleSystem(
    const std::string& template_name) {
  particles::ParticleSystemTemplate* tmpl =
      particles::ParticleSystemTemplate::GetOrLoad(template_name, video_);
  if (!tmpl) return;
  auto ps = std::make_unique<game::GameParticleSystem>(tmpl, video_);
  tmpl->Release();  // GameParticleSystem holds the AddRef'd reference
  BeginPreview(std::move(ps), 0.f, ImGuiMouseCursor_ResizeAll);
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

  // Gate orbit/pan/zoom input: block when the ViewManipulate widget is hovered.
  const bool widget_hovered = ImGuizmo::IsViewManipulateHovered();
  camera_ctrl_->SetViewportHovered(ImGui::IsWindowHovered() && !widget_hovered);
  camera_ctrl_->Update(ImGui::GetIO().DeltaTime);

  ResizeIfNeeded(w, h);

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

  // Placement mode: preview follows the cursor; LMB click confirms placement.
  if (scene_ && preview_active_ && ImGui::IsWindowHovered()) {
    ImGui::SetMouseCursor(preview_cursor_);
    UpdatePreviewPosition(ImGui::GetMousePos(), image_pos, avail);
    if (!ImGui::GetIO().KeyAlt && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
      PlacePreview();
  }

  // Sculpt brush: LMB drag when sculpt mode is active.
  // Overrides object picking for the duration of the stroke.
  if (sculpt_active_ && scene_ && ImGui::IsWindowHovered()) {
    const bool key_alt   = ImGui::GetIO().KeyAlt;
    const bool lmb_down  = ImGui::IsMouseDown(ImGuiMouseButton_Left) && !key_alt;

    if (lmb_down) {
      const auto hit = ComputeTerrainHit(ImGui::GetMousePos(), image_pos, avail);
      if (hit && on_sculpt_brush_) {
        const bool first = !sculpt_stroke_active_;
        sculpt_stroke_active_ = true;
        on_sculpt_brush_(hit->x, hit->z, first, ImGui::GetIO().DeltaTime);
      }
    } else if (sculpt_stroke_active_) {
      sculpt_stroke_active_ = false;
      if (on_sculpt_end_) on_sculpt_end_();
    }
  }

  // Delegate picking, gizmo drawing, and bounding-box drawing to the active tool.
  if (scene_ && selection_active_ && !sculpt_active_ && active_tool_base_) {
    const EditorToolContext ctx{scene_, camera_.get(), &picking_acc_,
                                history_, video_};
    active_tool_base_->OnRender(ctx, image_pos, avail);
  }

  // Light wireframes rendered into the scene FBO with depth testing so lights
  // behind opaque geometry are correctly occluded.
  if (scene_ && wireframe_fbo_) {
    light_wireframe_.Render(scene_->GetObjects(), scene_->GetSelectedObject(),
                            wireframe_fbo_.get());
  }

  // Player-start flag gizmos rendered without depth testing (always visible).
  if (scene_ && render_fbo_) {
    player_start_gizmo_.Render(scene_->GetObjects(), scene_->GetSelectedObject(),
                               render_fbo_.get());
  }

  // Particle system sphere gizmos rendered without depth testing.
  if (scene_ && render_fbo_) {
    particle_gizmo_.Render(scene_->GetObjects(), scene_->GetSelectedObject(),
                           render_fbo_.get());
  }

  // Terrain wireframe debug overlay — flat white edges over the composited scene.
  if (terrain_wireframe_debug_ && wireframe_fbo_) {
    renderer::Renderer::Instance().RenderTerrainWireframe(
        *camera_->GetCamera(), wireframe_fbo_.get());
  }

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

void EditorViewport::UpdatePreviewPosition(ImVec2 mouse_pos, ImVec2 image_pos,
                                            ImVec2 image_size) {
  const auto hit = ComputeTerrainHit(mouse_pos, image_pos, image_size);
  if (!hit) return;

  if (pending_preview_)
    preview_object_ = scene_->AddDynamicObject(std::move(pending_preview_));

  if (preview_object_) {
    preview_object_->SetWorldTransform(
        core::Mat4f::Translation({hit->x, hit->y + preview_height_, hit->z}));
    picking_acc_.UpdateMoved(preview_object_);
  }
}

void EditorViewport::PlacePreview() {
  if (!preview_object_) return;  // no valid floor hit yet

  if (history_) {
    auto obj = scene_->ReclaimDynamicObject(preview_object_);
    preview_object_ = nullptr;
    history_->Push(std::make_unique<PlaceObjectCommand>(scene_, std::move(obj)));
  } else {
    scene_->SetSelectedObject(preview_object_);
    preview_object_ = nullptr;
  }

  preview_active_ = false;
  SetSelectionActive(true);

  if (on_placement_done_) on_placement_done_();
}

void EditorViewport::PlaceMeshAt(ImVec2 mouse_pos, ImVec2 image_pos,
                                  ImVec2 image_size, game::MeshTemplate* tmpl) {
  if (!scene_ || !tmpl) return;

  const auto hit = ComputeTerrainHit(mouse_pos, image_pos, image_size);
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



std::optional<core::Vec3f> EditorViewport::ComputeTerrainHit(
    ImVec2 mouse_pos, ImVec2 image_pos, ImVec2 image_size) const {
  const float ndc_x = (mouse_pos.x - image_pos.x) / image_size.x * 2.f - 1.f;
  const float ndc_y = 1.f - (mouse_pos.y - image_pos.y) / image_size.y * 2.f;

  const core::Camera* cam        = camera_->GetCamera();
  const core::Vec3f   ray_origin = cam->GetPosition();
  const core::Mat4f   vp_inv     = cam->GetViewProjectionMatrix().Inverse();

  const core::Vec4f clip(ndc_x, ndc_y, -1.f, 1.f);
  const core::Vec4f world4 = clip * vp_inv;
  if (std::abs(world4.w) < 1e-6f) return std::nullopt;
  const core::Vec3f world3(world4.x / world4.w,
                           world4.y / world4.w,
                           world4.z / world4.w);
  const core::Vec3f ray_dir = (world3 - ray_origin).Normalized();

  // When no terrain data is available, fall back to the y=0 horizontal plane.
  if (!terrain_data_) {
    if (std::abs(ray_dir.y) < 1e-4f) return std::nullopt;
    const float t = -ray_origin.y / ray_dir.y;
    if (t < 0.f) return std::nullopt;
    return ray_origin + ray_dir * t;
  }

  // Ray-march against the heightmap with binary-search refinement.
  constexpr int   kSteps    = 64;
  constexpr float kMaxDist  = 2000.f;
  const float     step      = kMaxDist / kSteps;

  float prev_t = 0.f;
  for (int i = 0; i < kSteps; ++i) {
    const float  t = static_cast<float>(i + 1) * step;
    const core::Vec3f p = ray_origin + ray_dir * t;
    const float terrain_h = terrain_data_->GetHeight(p.x, p.z);
    if (p.y <= terrain_h) {
      // Binary search between prev_t and t.
      float lo = prev_t;
      float hi = t;
      for (int j = 0; j < 8; ++j) {
        const float mid   = (lo + hi) * 0.5f;
        const core::Vec3f mp   = ray_origin + ray_dir * mid;
        if (mp.y <= terrain_data_->GetHeight(mp.x, mp.z))
          hi = mid;
        else
          lo = mid;
      }
      return ray_origin + ray_dir * ((lo + hi) * 0.5f);
    }
    prev_t = t;
  }
  return std::nullopt;
}

EditorCameraController::CameraState EditorViewport::GetCameraState() const {
  return camera_ctrl_->GetState();
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

void EditorViewport::FrameObject(const core::BBox3& bbox) {
  camera_ctrl_->FrameObject(bbox);
}

void EditorViewport::SetActiveTool(EditorToolBase* tool) {
  if (!tool) tool = &selection_tool_;
  if (active_tool_base_)
    active_tool_base_->OnDeactivate();
  active_tool_base_ = tool;
  const EditorToolContext ctx{scene_, camera_.get(), &picking_acc_,
                              history_, video_};
  active_tool_base_->OnActivate(ctx);
}

}  // namespace editor
