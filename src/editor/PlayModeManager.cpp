#include "editor/PlayModeManager.h"

#include <loguru.hpp>

#include "core/Event.h"
#include "editor/EditorScene.h"
#include "editor/EditorToolbar.h"
#include "editor/EditorViewport.h"
#include "editor/MapSerializer.h"
#include "game/FPSCameraController.h"
#include "game/GameMesh.h"
#include "game/GamePlayerStart.h"
#include "game/GameTerrain.h"
#include "physics/PhysicsBody.h"
#include "physics/PhysicsBodyDesc.h"
#include "physics/PhysicsSystem.h"
#include "physics/PhysicsShapeDesc.h"

namespace editor {

// ---- EnterVisitor -----------------------------------------------------------

void PlayModeManager::EnterVisitor::Visit(game::GameMesh& mesh) {
  if (!physics::PhysicsSystem::IsInstanced()) return;

  // Skip meshes that already have a physics body (managed by GameMesh itself).
  if (mesh.GetPhysicsBody()) return;

  physics::PhysicsBodyDesc desc;
  desc.motion_type = physics::MotionType::Static;

  const auto& positions = mesh.GetTemplate()->GetCPUPositions();
  const auto& indices   = mesh.GetTemplate()->GetCPUIndices();

  physics::PhysicsBody* body = nullptr;
  if (!positions.empty() && !indices.empty()) {
    desc.shape = physics::PhysicsShapeDesc::MakeConvexHull();
    body = physics::PhysicsSystem::Instance().CreateBodyWithMesh(
        desc, nullptr, mesh.GetWorldTransform(),
        reinterpret_cast<const float*>(positions.data()),
        static_cast<int>(positions.size()),
        indices.data(),
        static_cast<int>(indices.size()),
        mesh.GetTemplate());
  } else {
    // Fallback: axis-aligned box from the mesh world bbox half-extents.
    const core::BBox3& wb = mesh.GetWorldBBox();
    const core::Vec3f  half = wb.GetSize() * 0.5f;
    desc.shape = physics::PhysicsShapeDesc::MakeBox(half);
    body = physics::PhysicsSystem::Instance().CreateBody(
        desc, nullptr, mesh.GetWorldTransform());
  }

  if (body) created_bodies.push_back(body);
}

void PlayModeManager::EnterVisitor::Visit(game::GamePlayerStart& ps) {
  if (!player_start) player_start = &ps;
}

void PlayModeManager::EnterVisitor::Visit(game::GameTerrain& t) {
  terrain = &t;
}

// ---- PlayModeManager --------------------------------------------------------

PlayModeManager::PlayModeManager(EditorScene* scene,
                                 EditorToolbar* toolbar,
                                 EditorViewport* viewport,
                                 abstract::VideoDevice* video)
    : scene_(scene),
      toolbar_(toolbar),
      viewport_(viewport),
      video_(video),
      saved_camera_state_({}) {}

PlayModeManager::~PlayModeManager() {
  if (playing_) Exit();
}

void PlayModeManager::SetOnSceneReloaded(
    std::function<void(std::unique_ptr<EditorScene>,
                       EditorCameraController::CameraState)> cb) {
  on_scene_reloaded_ = std::move(cb);
}

void PlayModeManager::SetOnStatusMessage(std::function<void(std::string)> cb) {
  on_status_message_ = std::move(cb);
}

void PlayModeManager::SetStatusMessage(const std::string& msg) {
  if (on_status_message_) on_status_message_(msg);
}

void PlayModeManager::Enter() {
  if (playing_) return;

  // 1. Validate file path.
  if (scene_->GetFilePath().empty()) {
    SetStatusMessage("Save the map before entering Play mode");
    return;
  }

  // 2. Validate player start via visitor.
  EnterVisitor visitor;
  for (game::GameObject* obj : scene_->GetObjects())
    obj->Accept(visitor);

  if (!visitor.player_start) {
    SetStatusMessage("Place a Player Start before entering Play mode");
    return;
  }

  // 3. Auto-save.
  const EditorCameraController::CameraState cam_state = viewport_->GetCameraState();
  if (!MapSerializer::Save(*scene_, cam_state, scene_->GetFilePath())) {
    SetStatusMessage("Failed to save the map — Play mode aborted");
    return;
  }
  saved_camera_state_ = cam_state;

  // 4. Deactivate editor tools.
  toolbar_->SetActiveTool(EditorTool::kSelection);
  tools_frozen_  = true;

  // 5. Hide editor panels.
  panels_hidden_ = true;

  // 6. Show HUD — UIRenderer is not instanced in the editor app so we rely
  //    on the panels_hidden_ flag checked by EditorWindow instead.

  // 7. Instantiate FPS camera at the player start world position.
  fps_controller_ = std::make_unique<game::FPSCameraController>();
  fps_controller_->SetCamera(viewport_->GetCamera());
  const core::Mat4f& t = visitor.player_start->GetWorldTransform();
  fps_controller_->SetPosition({t(0, 3), t(1, 3), t(2, 3)});

  // Freeze the editor orbit camera controller so it does not override the
  // FPS camera transform inside EditorViewport::Render().
  viewport_->SetInPlayMode(true);

  // 8. Register static physics bodies for meshes.
  play_bodies_ = std::move(visitor.created_bodies);

  // 9. The terrain body is already managed by GameTerrain::OnAddedToScene();
  //    we do not create a second body and do not store a handle here.

  // 10.
  playing_ = true;

  LOG_F(INFO, "Play mode entered");
}

void PlayModeManager::Exit() {
  if (!playing_) return;

  // 1. Remove play-time mesh physics bodies.
  if (physics::PhysicsSystem::IsInstanced()) {
    for (physics::PhysicsBody* b : play_bodies_)
      physics::PhysicsSystem::Instance().DestroyBody(b);
  }
  play_bodies_.clear();

  // 2. Restore scene from disk.
  auto result = MapSerializer::Load(scene_->GetFilePath(), video_);
  if (result && on_scene_reloaded_) {
    on_scene_reloaded_(std::move(result->scene), saved_camera_state_);
  } else if (!result) {
    LOG_F(ERROR, "Play mode exit: failed to reload map '%s'",
          scene_->GetFilePath().c_str());
  }

  // 3. Restore editor camera controller.
  viewport_->SetInPlayMode(false);
  viewport_->SetCameraState(saved_camera_state_);

  // Destroy the FPS controller after restoring the viewport so no dangling
  // pointer is held in EditorViewport during the transition.
  fps_controller_.reset();

  // 4 & 5. Restore panels and tools.
  panels_hidden_ = false;
  tools_frozen_  = false;

  // 6. Hide HUD (no-op — UIRenderer not instanced in the editor app).

  // 7.
  toolbar_->SetInPlayMode(false);

  // 8.
  playing_ = false;

  LOG_F(INFO, "Play mode exited");
}

void PlayModeManager::Tick(float dt) {
  if (!playing_) return;

  if (physics::PhysicsSystem::IsInstanced())
    physics::PhysicsSystem::Instance().Step(dt);

  if (fps_controller_)
    fps_controller_->Update(dt);
}

void PlayModeManager::OnEvent(const core::Event& event) {
  if (fps_controller_)
    fps_controller_->OnEvent(event);
}

}  // namespace editor
