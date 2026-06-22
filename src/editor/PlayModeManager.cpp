#include "editor/PlayModeManager.h"

#include <loguru.hpp>

#include "core/Event.h"
#include "core/GpuProfiler.h"
#include "core/Profiler.h"
#include "editor/EditorScene.h"
#include "editor/EditorToolbar.h"
#include "editor/EditorViewport.h"
#include "editor/MapSerializer.h"
#include "game/ChaseCameraController.h"
#include "game/GameMesh.h"
#include "game/GamePlayerStart.h"
#include "game/GameTerrain.h"
#include "game/GameVehicle.h"
#include "game/PlayerVehicleController.h"
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

void PlayModeManager::EnterVisitor::Visit(game::GameVehicle& v) {
  if (!vehicle) vehicle = &v;
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

void PlayModeManager::SetOnExit(
    std::function<void(std::filesystem::path,
                       EditorCameraController::CameraState)> cb) {
  on_exit_ = std::move(cb);
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

  if (!visitor.vehicle) {
    SetStatusMessage("Place a Vehicle in the scene before entering Play mode");
    return;
  }

  // 3. Auto-save.
  const EditorCameraController::CameraState cam_state = viewport_->GetCameraState();
  if (!MapSerializer::Save(*scene_, cam_state, scene_->GetFilePath())) {
    SetStatusMessage("Failed to save the map — Play mode aborted");
    return;
  }
  saved_camera_state_ = cam_state;
  saved_file_path_    = scene_->GetFilePath();

  // 4. Deactivate editor tools.
  toolbar_->SetActiveTool(EditorTool::kSelection);
  tools_frozen_  = true;

  // 5. Hide editor panels.
  panels_hidden_ = true;

  // 6. Show HUD — UIRenderer is not instanced in the editor app so we rely
  //    on the panels_hidden_ flag checked by EditorWindow instead.

  // 7. Activate the vehicle and wire player input + chase camera.
  vehicle_ = visitor.vehicle;
  vehicle_controller_ = std::make_unique<game::PlayerVehicleController>();
  vehicle_->SetVehicleController(vehicle_controller_.get());
  vehicle_->Activate();

  chase_controller_ = std::make_unique<game::ChaseCameraController>();
  chase_controller_->SetCamera(viewport_->GetCamera());
  chase_controller_->SetTarget(vehicle_);

  // Freeze the editor orbit camera controller so it does not override the
  // chase camera transform inside EditorViewport::Render().
  viewport_->SetInPlayMode(true);

  // 8. Register static physics bodies for meshes.
  play_bodies_ = std::move(visitor.created_bodies);

  // 9. The terrain body is already managed by GameTerrain::OnAddedToScene();
  //    we do not create a second body and do not store a handle here.

  playing_ = true;

  // 11. Activate profilers for play-time performance measurement.
  new core::Profiler(kProfilerInterval);
  new core::GpuProfiler(kProfilerInterval);

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

  // 2. Release the vehicle controller and clear its reference on the vehicle so
  //    no dangling callback fires between here and scene destruction.
  if (vehicle_) vehicle_->SetVehicleController(nullptr);
  vehicle_controller_.reset();
  vehicle_ = nullptr;

  // 3. Fire the on_exit_ callback so the caller can reset the old scene and
  // load a fresh one. The caller must destroy the old scene BEFORE calling
  // MapSerializer::Load() to prevent FoliageRenderer double-instantiation
  // (GameTerrain::OnRemovedFromScene() destroys the singleton only when the
  // old scene is destroyed; if Load() runs while it still exists, the new
  // GameTerrain triggers the singleton assert).
  if (on_exit_) {
    on_exit_(saved_file_path_, saved_camera_state_);
  }

  // 4. Restore editor camera controller.
  viewport_->SetInPlayMode(false);

  // Destroy the chase controller after restoring the viewport so no dangling
  // pointer is held in EditorViewport during the transition.
  chase_controller_.reset();

  // 4 & 5. Restore panels and tools.
  panels_hidden_ = false;
  tools_frozen_  = false;

  // 6. Hide HUD (no-op — UIRenderer not instanced in the editor app).

  // 7.
  toolbar_->SetInPlayMode(false);

  // 8.
  playing_ = false;

  // 9. Shut down profilers before the next Logger flush.
  if (core::GpuProfiler::IsInstanced()) core::GpuProfiler::Shutdown();
  if (core::Profiler::IsInstanced())    core::Profiler::Shutdown();

  LOG_F(INFO, "Play mode exited");
}

void PlayModeManager::Tick(float dt) {
  if (!playing_) return;

  PROFILE_SCOPE("PlayModeManager::Tick");

  // Collect GPU timings submitted during the previous frame's Renderer::Update.
  // Called at the top of Tick (before the current frame's render) so the GPU
  // has had a full frame to complete the previous submissions.
  if (core::GpuProfiler::IsInstanced()) {
    const auto gpu_samples = video_->CollectGpuTimings();
    if (!gpu_samples.empty())
      core::GpuProfiler::Instance().RecordSamples(gpu_samples);
    core::GpuProfiler::Instance().MarkFrame();
  }

  if (core::Profiler::IsInstanced())
    core::Profiler::Instance().MarkFrame();

  {
    // cppcheck-suppress shadowVariable
    PROFILE_SCOPE("Physics::Step");
    if (physics::PhysicsSystem::IsInstanced())
      physics::PhysicsSystem::Instance().Step(dt);
  }

  // vehicle_->Update() internally calls controller_->Update(dt) before feeding
  // inputs to the physics vehicle, so no separate controller update is needed.
  if (vehicle_)
    vehicle_->Update(dt);

  if (chase_controller_)
    chase_controller_->Update(dt);
}

void PlayModeManager::OnEvent(const core::Event& event) {
  if (vehicle_controller_)
    vehicle_controller_->OnEvent(event);
}

}  // namespace editor
