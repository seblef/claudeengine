#include "game/GameVehicle.h"

#include <loguru.hpp>

#include "core/MathUtils.h"
#include "game/GameMesh.h"
#include "game/GameObjectVisitor.h"
#include "game/IVehicleController.h"
#include "game/VehicleTemplate.h"
#include "physics/PhysicsSystem.h"
#include "physics/PhysicsVehicle.h"
#include "renderer/Renderer.h"
#include "track/TireTrackSystem.h"

namespace game {

namespace {

constexpr float kReverseEngageDelay    = 0.3f;   ///< Brake-hold time (s) before reverse engages.
constexpr float kReverseSpeedThreshold = 0.3f;   ///< Speed (m/s) below which the car is considered stopped.
constexpr float kReverseThrottle       = 0.6f;   ///< Fraction of max engine torque applied in reverse.

constexpr float kFlipDetectionThreshold = -0.5f;  ///< dot(vehicle_up, world_up) below which the vehicle is flipped.
constexpr float kFlipSpeedThreshold     =  1.0f;  ///< Speed (m/s) below which flip detection is active.
constexpr float kFlipDelay              =  2.0f;  ///< Time (s) upside-down before auto self-right triggers.
constexpr float kSelfRightLiftOffset    =  0.5f;  ///< Upward offset (m) applied to position on self-right.
constexpr float kRecoveryDuration       =  1.5f;  ///< Duration (s) of the post-self-right visibility flicker.
constexpr float kFlickerFrequency       =  8.0f;  ///< Flicker rate (Hz) during recovery.

// A negative-scale mirror would flip winding order and break back-face culling.
// A 180° Y rotation achieves the same visual mirroring with det=+1.
core::Mat4f MirrorY() {
  return core::Mat4f::RotationY(core::kPi);
}

core::Mat4f PositionMatrix(const core::Vec3f& p) {
  core::Mat4f m = core::Mat4f::kIdentity;
  m(0, 3) = p.x;
  m(1, 3) = p.y;
  m(2, 3) = p.z;
  return m;
}

}  // namespace

GameVehicle::GameVehicle(VehicleTemplate* tmpl)
    : GameObject(GameObjectType::kVehicle,
                 tmpl->GetBodyTemplate()->GetLocalBBox()),
      template_(tmpl) {
  template_->AddRef();

  body_mesh_ = std::make_unique<GameMesh>(tmpl->GetBodyTemplate());
  wheel_fl_  = std::make_unique<GameMesh>(tmpl->GetFrontWheelTemplate());
  wheel_fr_  = std::make_unique<GameMesh>(tmpl->GetFrontWheelTemplate());
  wheel_rl_  = std::make_unique<GameMesh>(tmpl->GetRearWheelTemplate());
  wheel_rr_  = std::make_unique<GameMesh>(tmpl->GetRearWheelTemplate());

  AddChild(body_mesh_.get());
  AddChild(wheel_fl_.get());
  AddChild(wheel_fr_.get());
  AddChild(wheel_rl_.get());
  AddChild(wheel_rr_.get());

  const physics::VehicleDesc& vdesc = tmpl->GetVehicleDesc();
  const core::Mat4f mirror = MirrorY();
  wheel_fl_->SetLocalTransform(PositionMatrix(vdesc.front_left.position));
  wheel_fr_->SetLocalTransform(PositionMatrix(vdesc.front_right.position) * mirror);
  wheel_rl_->SetLocalTransform(PositionMatrix(vdesc.rear_left.position));
  wheel_rr_->SetLocalTransform(PositionMatrix(vdesc.rear_right.position) * mirror);
}

GameVehicle::~GameVehicle() {
  template_->Release();
}

void GameVehicle::Accept(GameObjectVisitor& visitor) {
  visitor.Visit(*this);
}

std::unique_ptr<GameObject> GameVehicle::Copy(
    const core::Vec3f& position) const {
  auto clone = std::make_unique<GameVehicle>(template_);
  clone->SetName(GetName());
  core::Mat4f t = GetWorldTransform();
  t(0, 3) = position.x;
  t(1, 3) = position.y;
  t(2, 3) = position.z;
  clone->SetWorldTransform(t);
  return clone;
}

void GameVehicle::OnAddedToScene() {
  body_mesh_->OnAddedToScene();
  wheel_fl_->OnAddedToScene();
  wheel_fr_->OnAddedToScene();
  wheel_rl_->OnAddedToScene();
  wheel_rr_->OnAddedToScene();
}

void GameVehicle::OnRemovedFromScene() {
  Deactivate();
  body_mesh_->OnRemovedFromScene();
  wheel_fl_->OnRemovedFromScene();
  wheel_fr_->OnRemovedFromScene();
  wheel_rl_->OnRemovedFromScene();
  wheel_rr_->OnRemovedFromScene();
}

void GameVehicle::Activate() {
  if (physics_vehicle_) return;
  if (!physics::PhysicsSystem::IsInstanced()) {
    LOG_F(WARNING, "GameVehicle::Activate: PhysicsSystem not available");
    return;
  }
  const core::Vec3f* body_verts  = nullptr;
  int                body_count  = 0;
  if (template_->UseConvexHullBody()) {
    const auto& positions = template_->GetBodyTemplate()->GetCPUPositions();
    body_verts = positions.data();
    body_count = static_cast<int>(positions.size());
  }
  physics_vehicle_ = physics::PhysicsSystem::Instance().CreateVehicle(
      template_->GetVehicleDesc(), this, GetWorldTransform(),
      template_->GetFrontWheelGeometry(), template_->GetRearWheelGeometry(),
      body_verts, body_count);

  if (renderer::Renderer::IsInstanced()) {
    track::TireTrackSystem* tts =
        renderer::Renderer::Instance().GetTireTrackSystem();
    if (tts) tts->RegisterVehicle(physics_vehicle_);
  }
}

void GameVehicle::Deactivate() {
  if (!physics_vehicle_) return;
  if (renderer::Renderer::IsInstanced()) {
    track::TireTrackSystem* tts =
        renderer::Renderer::Instance().GetTireTrackSystem();
    if (tts) tts->UnregisterVehicle(physics_vehicle_);
  }
  if (physics::PhysicsSystem::IsInstanced())
    physics::PhysicsSystem::Instance().DestroyVehicle(physics_vehicle_);
  physics_vehicle_ = nullptr;
}

void GameVehicle::Update(float dt) {
  if (physics_vehicle_) {
    const core::Mat4f transform = physics_vehicle_->GetBodyWorldTransform();
    const float       speed     = physics_vehicle_->GetForwardSpeed();

    // --- Flip state machine ---------------------------------------------------
    // Column 1 of the local-to-world transform is the vehicle's local Y axis
    // expressed in world space.
    const core::Vec3f vehicle_up(transform(0, 1), transform(1, 1), transform(2, 1));
    const float up_dot = vehicle_up.Normalized().Dot(core::Vec3f::kAxisY);

    switch (drive_state_) {
      case DriveState::kForward:
      case DriveState::kBraking:
      case DriveState::kReverse:
        if (up_dot < kFlipDetectionThreshold &&
            std::fabs(speed) < kFlipSpeedThreshold) {
          drive_state_ = DriveState::kFlipped;
          flip_timer_  = 0.f;
        }
        break;

      case DriveState::kFlipped:
        if (up_dot >= kFlipDetectionThreshold) {
          // Player rocked the vehicle back naturally — no flicker needed.
          drive_state_ = DriveState::kForward;
          flip_timer_  = 0.f;
        } else {
          flip_timer_ += dt;
          if (flip_timer_ >= kFlipDelay)
            SelfRight();
        }
        break;

      case DriveState::kRecovering:
        recovery_timer_ -= dt;
        if (recovery_timer_ <= 0.f) {
          drive_state_ = DriveState::kForward;
          SetMeshesVisible(true);
        } else {
          const bool visible =
              (static_cast<int>(recovery_timer_ * kFlickerFrequency * 2.f) % 2) == 0;
          SetMeshesVisible(visible);
        }
        break;
    }

    // --- Driver input --------------------------------------------------------
    // Inputs are suppressed while the vehicle is flipped (stuck) or recovering.
    if (controller_ &&
        drive_state_ != DriveState::kFlipped &&
        drive_state_ != DriveState::kRecovering) {
      controller_->Update(dt);

      const float throttle = controller_->GetThrottle();
      const float brake    = controller_->GetBrake();

      switch (drive_state_) {
        case DriveState::kForward:
          physics_vehicle_->SetThrottle(throttle);
          physics_vehicle_->SetBrake(brake);
          if (brake > 0.f && speed < kReverseSpeedThreshold) {
            drive_state_   = DriveState::kBraking;
            reverse_timer_ = 0.f;
          }
          break;

        case DriveState::kBraking:
          physics_vehicle_->SetThrottle(0.f);
          physics_vehicle_->SetBrake(brake);
          if (brake == 0.f || throttle > 0.f) {
            drive_state_ = DriveState::kForward;
          } else if (speed < kReverseSpeedThreshold) {
            reverse_timer_ += dt;
            if (reverse_timer_ >= kReverseEngageDelay)
              drive_state_ = DriveState::kReverse;
          } else {
            reverse_timer_ = 0.f;
          }
          break;

        case DriveState::kReverse:
          physics_vehicle_->SetThrottle(-kReverseThrottle);
          physics_vehicle_->SetBrake(0.f);
          if (brake == 0.f || throttle > 0.f) drive_state_ = DriveState::kForward;
          break;

        case DriveState::kFlipped:
        case DriveState::kRecovering:
          break;
      }

      physics_vehicle_->SetSteer(controller_->GetSteer());
      physics_vehicle_->SetHandbrake(controller_->GetHandbrake());
    } else if (drive_state_ == DriveState::kFlipped) {
      physics_vehicle_->SetThrottle(0.f);
      physics_vehicle_->SetBrake(0.f);
      physics_vehicle_->SetSteer(0.f);
      physics_vehicle_->SetHandbrake(false);
    }

    // --- Wheel transforms ----------------------------------------------------
    const core::Mat4f body_world_inv = GetWorldTransform().Inverse();
    const core::Mat4f mirror         = MirrorY();

    GameMesh* wheels[4] = {
        wheel_fl_.get(), wheel_fr_.get(), wheel_rl_.get(), wheel_rr_.get()
    };
    const bool mirrored[4] = {false, true, false, true};

    for (int i = 0; i < 4; ++i) {
      const core::Mat4f wheel_world = physics_vehicle_->GetWheelWorldTransform(i);
      const core::Mat4f wheel_local = body_world_inv * wheel_world;
      wheels[i]->SetLocalTransform(
          mirrored[i] ? wheel_local * mirror : wheel_local);
    }
  }

  GameObject::Update(dt);
}

void GameVehicle::SelfRight() {
  const core::Mat4f transform = physics_vehicle_->GetBodyWorldTransform();

  // Column 2 of the local-to-world transform is the vehicle's local Z (forward)
  // in world space.  Project onto the XZ plane to extract yaw.
  const float fx = transform(0, 2);
  const float fz = transform(2, 2);
  const float fw_len = std::sqrt(fx * fx + fz * fz);
  // atan2(fx, fz): sin(yaw) = fx, cos(yaw) = fz for RotationY(yaw).
  const float yaw = (fw_len > 1e-4f) ? std::atan2(fx, fz) : 0.f;

  // Build upright transform: yaw rotation only, lifted above current position.
  core::Mat4f upright = core::Mat4f::RotationY(yaw);
  upright(0, 3) = transform(0, 3);
  upright(1, 3) = transform(1, 3) + kSelfRightLiftOffset;
  upright(2, 3) = transform(2, 3);

  physics_vehicle_->SetBodyTransform(upright);
  physics_vehicle_->ZeroVelocities();

  drive_state_    = DriveState::kRecovering;
  recovery_timer_ = kRecoveryDuration;
  flip_timer_     = 0.f;

  LOG_F(INFO, "GameVehicle: self-righted at yaw=%.2f rad", yaw);
}

void GameVehicle::SetMeshesVisible(bool visible) {
  body_mesh_->SetVisible(visible);
  wheel_fl_->SetVisible(visible);
  wheel_fr_->SetVisible(visible);
  wheel_rl_->SetVisible(visible);
  wheel_rr_->SetVisible(visible);
}

void GameVehicle::OnBodyTransformUpdated(const core::Mat4f& transform) {
  SetWorldTransformPhysics(transform);
}

std::filesystem::path GameVehicle::GetDescPath() const {
  return std::filesystem::path(template_->GetDescPath());
}

const physics::VehicleDesc& GameVehicle::GetVehicleDesc() const {
  return template_->GetVehicleDesc();
}

MeshTemplate* GameVehicle::GetBodyTemplate() const {
  return template_->GetBodyTemplate();
}

}  // namespace game
