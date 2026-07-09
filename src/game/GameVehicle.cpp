#include "game/GameVehicle.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

#include <loguru.hpp>

#include "core/MathUtils.h"
#include "core/RayUtils.h"
#include "game/GameMesh.h"
#include "game/GameObjectVisitor.h"
#include "game/IVehicleController.h"
#include "game/IVehicleFireListener.h"
#include "game/IVehicleScrapeListener.h"
#include "game/IVehicleWreckListener.h"
#include "game/VehicleCrashSound.h"
#include "game/VehicleDamage.h"
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

// The last configured damage threshold conventionally marks a full wreck
// (see physics::VehicleDamageDesc::thresholds' default {0.4, 0.6, 0.8, 1.0}).
constexpr float kWreckThreshold = 1.f;

// A negative-scale mirror would flip winding order and break back-face culling.
// A 180° Y rotation achieves the same visual mirroring with det=+1.
core::Mat4f MirrorY() {
  return core::Mat4f::RotationY(core::kPi);
}

// Linearly ramps steer authority down from 1.0 at 0 m/s to desc.min_steer_scale
// at desc.high_speed_reference_speed and above.
float ComputeSteerScale(float speed, const physics::VehicleDesc& desc) {
  const float t = std::clamp(std::fabs(speed) / desc.high_speed_reference_speed, 0.f, 1.f);
  return std::lerp(1.f, desc.min_steer_scale, t);
}

// Returns the scale of the highest enabled threshold at or below fraction, or
// 1.0 (no effect) when none apply. thresholds need not be sorted.
float ComputeDamageEffectScale(float fraction, const std::array<float, 4>& thresholds,
                               const std::array<float, 4>& scales,
                               const std::array<bool, 4>& enabled) {
  float scale         = 1.f;
  float best_threshold = -1.f;
  for (size_t i = 0; i < thresholds.size(); ++i) {
    if (enabled[i] && fraction >= thresholds[i] && thresholds[i] > best_threshold) {
      best_threshold = thresholds[i];
      scale          = scales[i];
    }
  }
  return scale;
}

core::Mat4f PositionMatrix(const core::Vec3f& p) {
  core::Mat4f m = core::Mat4f::kIdentity;
  m(0, 3) = p.x;
  m(1, 3) = p.y;
  m(2, 3) = p.z;
  return m;
}

}  // namespace

GameVehicle::GameVehicle(VehicleTemplate* tmpl,
                         audio::SoundManager* sound_manager,
                         audio::ResourceManager* resource_manager)
    : GameObject(GameObjectType::kVehicle,
                 tmpl->GetBodyTemplate()->GetLocalBBox()),
      template_(tmpl),
      damage_(std::make_unique<VehicleDamage>(tmpl->GetVehicleDesc())),
      crash_sound_(std::make_unique<VehicleCrashSound>(
          tmpl->GetVehicleDesc().crash_sound, sound_manager, resource_manager)),
      sound_manager_(sound_manager),
      resource_manager_(resource_manager) {
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

  damage_->AddListener(this);
}

GameVehicle::~GameVehicle() {
  template_->Release();
}

void GameVehicle::Accept(GameObjectVisitor& visitor) {
  visitor.Visit(*this);
}

std::unique_ptr<GameObject> GameVehicle::Copy(
    const core::Vec3f& position) const {
  auto clone = std::make_unique<GameVehicle>(
      template_, sound_manager_, resource_manager_);
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
      body_verts, body_count, this);

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
  // Deliver a wreck notification deferred from OnDamageThresholdCrossed()
  // (see its doc comment in GameVehicle.h). This runs before this frame's
  // physics::PhysicsSystem::Step(), so it is always outside any Jolt contact
  // callback — safe for wreck_listener_ to issue physics queries (e.g. the
  // explosion's shockwave SphereOverlap).
  if (wreck_notify_pending_) {
    wreck_notify_pending_ = false;
    if (wreck_listener_) wreck_listener_->OnVehicleWrecked(wreck_position_);
  }

  if (physics_vehicle_) {
    crash_sound_->Update(dt);
    if (scrape_listener_) scrape_listener_->Update(dt);
    UpdateDamageEffects();
    UpdateBodyMeshVariant();

    const core::Mat4f transform = physics_vehicle_->GetBodyWorldTransform();
    const float       speed     = physics_vehicle_->GetForwardSpeed();

    if (fire_listener_) fire_listener_->OnVehicleTransformUpdated(dt, transform);

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

      case DriveState::kWrecked:
        // A wreck is inert: no self-righting, no flip detection.
        break;
    }

    // --- Driver input --------------------------------------------------------
    // Inputs are suppressed while the vehicle is flipped (stuck), recovering,
    // or wrecked (dead).
    if (controller_ &&
        drive_state_ != DriveState::kFlipped &&
        drive_state_ != DriveState::kRecovering &&
        drive_state_ != DriveState::kWrecked) {
      controller_->Update(dt);

      const float throttle = controller_->GetThrottle();
      const float brake    = controller_->GetBrake();

      switch (drive_state_) {
        case DriveState::kForward:
          physics_vehicle_->SetThrottle(throttle * damage_speed_scale_);
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
          physics_vehicle_->SetThrottle(-kReverseThrottle * damage_speed_scale_);
          physics_vehicle_->SetBrake(0.f);
          if (brake == 0.f || throttle > 0.f) drive_state_ = DriveState::kForward;
          break;

        case DriveState::kFlipped:
        case DriveState::kRecovering:
        case DriveState::kWrecked:
          break;
      }

      const float steer_scale =
          ComputeSteerScale(speed, GetVehicleDesc()) * damage_steer_scale_;
      physics_vehicle_->SetSteer(controller_->GetSteer() * steer_scale);
      physics_vehicle_->SetHandbrake(controller_->GetHandbrake());
    } else if (drive_state_ == DriveState::kFlipped ||
              drive_state_ == DriveState::kWrecked) {
      physics_vehicle_->SetThrottle(0.f);
      physics_vehicle_->SetBrake(0.f);
      physics_vehicle_->SetSteer(0.f);
      physics_vehicle_->SetHandbrake(false);
    }

    // --- Wheel transforms ----------------------------------------------------
    // Frozen once wrecked: a wreck no longer "drives", so its wheels stop
    // being re-posed from the physics simulation.
    if (drive_state_ != DriveState::kWrecked) {
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

void GameVehicle::UpdateDamageEffects() {
  const physics::VehicleDamageDesc& damage_desc = GetVehicleDesc().damage;
  const float front_fraction = damage_->GetDamageFraction(DamageZone::kFront);

  damage_steer_scale_ = ComputeDamageEffectScale(
      front_fraction, damage_desc.thresholds,
      damage_desc.effects.steering_scale, damage_desc.effects.steering_enabled);
  damage_speed_scale_ = ComputeDamageEffectScale(
      front_fraction, damage_desc.thresholds,
      damage_desc.effects.speed_scale, damage_desc.effects.speed_enabled);
}

void GameVehicle::UpdateBodyMeshVariant() {
  const auto& variants = GetVehicleDesc().damage.mesh_variants;
  if (variants.empty()) return;

  const float avg_fraction = damage_->GetAverageDamageFraction();
  int   desired         = -1;
  float best_threshold  = -1.f;
  for (size_t i = 0; i < variants.size(); ++i) {
    if (avg_fraction >= variants[i].threshold && variants[i].threshold > best_threshold) {
      best_threshold = variants[i].threshold;
      desired        = static_cast<int>(i);
    }
  }
  if (desired == current_mesh_variant_) return;

  MeshTemplate* tmpl = desired >= 0
      ? template_->GetBodyDamageVariantTemplate(static_cast<size_t>(desired))
      : template_->GetBodyTemplate();
  if (tmpl) {
    body_mesh_->SetTemplate(tmpl);
    current_mesh_variant_ = desired;
  }
}

void GameVehicle::OnBodyTransformUpdated(const core::Mat4f& transform) {
  SetWorldTransformPhysics(transform);
}

void GameVehicle::OnCollision(const core::Vec3f& world_point, float impulse) {
  if (drive_state_ == DriveState::kWrecked) return;  // Inert: no further damage/sound events.

  const core::Vec3f local_point =
      core::TransformPoint(GetWorldTransform().Inverse(), world_point);
  damage_->RegisterImpact(local_point, impulse);
  crash_sound_->RegisterImpact(world_point, impulse);
}

void GameVehicle::OnDamageThresholdCrossed(DamageZone /*zone*/, float threshold,
                                          float /*fraction*/) {
  if (threshold < kWreckThreshold) return;
  if (drive_state_ == DriveState::kWrecked) return;  // Already wrecked; ignore later zones.

  drive_state_ = DriveState::kWrecked;

  const core::Mat4f& world_transform = GetWorldTransform();
  wreck_position_ = core::Vec3f{
      world_transform(0, 3), world_transform(1, 3), world_transform(2, 3)};

  LOG_F(WARNING, "GameVehicle: wrecked at (%.1f, %.1f, %.1f)",
        wreck_position_.x, wreck_position_.y, wreck_position_.z);

  // Deferred to the next Update() — see this method's doc comment in
  // GameVehicle.h for why wreck_listener_ cannot be notified from here.
  wreck_notify_pending_ = true;
}

void GameVehicle::OnSustainedContact(const core::Vec3f& world_point,
                                     const core::Vec3f& world_normal,
                                     const core::Vec3f& relative_velocity) {
  if (scrape_listener_)
    scrape_listener_->RegisterContact(world_point, world_normal, relative_velocity);
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
