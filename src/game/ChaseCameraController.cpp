#include "game/ChaseCameraController.h"

#include <cmath>

#include "core/EventType.h"
#include "core/Key.h"
#include "core/Mat4f.h"
#include "core/Vec3f.h"
#include "game/GameCamera.h"
#include "game/GameObject.h"
#include "physics/CollisionLayer.h"
#include "physics/PhysicsSystem.h"
#include "physics/RaycastResult.h"

namespace game {

ChaseCameraController::ChaseCameraController()
    : position_(core::Vec3f::kZero) {}

void ChaseCameraController::SetCamera(GameCamera* camera) {
  camera_ = camera;
}

void ChaseCameraController::SetTarget(const GameObject* target) {
  target_ = target;
}

void ChaseCameraController::SetArmLength(float m)           { arm_length_       = m; }
void ChaseCameraController::SetArmHeight(float m)           { arm_height_       = m; }
void ChaseCameraController::SetSpringStiffness(float k)     { spring_stiffness_ = k; }
void ChaseCameraController::SetOrbitSpeed(float rad_per_sec){ orbit_speed_      = rad_per_sec; }

void ChaseCameraController::OnEvent(const core::Event& event) {
  switch (event.type) {
    case core::EventType::kKeyDown:
      if (event.key == core::Key::kU) orbit_left_held_  = true;
      if (event.key == core::Key::kI) orbit_right_held_ = true;
      if (event.key == core::Key::kR) yaw_offset_       = 0.f;
      break;
    case core::EventType::kKeyUp:
      if (event.key == core::Key::kU) orbit_left_held_  = false;
      if (event.key == core::Key::kI) orbit_right_held_ = false;
      break;
    default:
      break;
  }
}

void ChaseCameraController::Update(float dt) {
  if (!camera_ || !target_) return;

  const core::Mat4f& world = target_->GetWorldTransform();

  const core::Vec3f target_pos     = {world(0, 3), world(1, 3), world(2, 3)};
  // Column 2 of a game-object world matrix is the true +Z forward direction.
  // (Unlike the camera matrix, which stores -forward in column 2.)
  const core::Vec3f target_forward = {world(0, 2), world(1, 2), world(2, 2)};

  // Accumulate orbit offset from held keys.
  if (orbit_left_held_)  yaw_offset_ -= orbit_speed_ * dt;
  if (orbit_right_held_) yaw_offset_ += orbit_speed_ * dt;

  // Scale arm by object size so the camera always clears the target's extent.
  const core::Vec3f bbox_size   = target_->GetLocalBBox().GetSize();
  const float       half_diag   = bbox_size.Length() * 0.5f;
  const float       eff_length  = arm_length_ + half_diag;
  const float       eff_height  = arm_height_ + bbox_size.y * 0.5f;

  // Rotate the arm direction by the yaw offset (Y-axis rotation in world space,
  // applied to the target's XZ forward vector so offset stays car-relative).
  const float       c          = std::cos(yaw_offset_);
  const float       s          = std::sin(yaw_offset_);
  const core::Vec3f orbit_dir  = core::Vec3f{
      target_forward.x * c + target_forward.z * s,
      0.f,
      -target_forward.x * s + target_forward.z * c
  }.Normalized();

  // Desired position: behind and above the target along the orbit direction.
  core::Vec3f desired = target_pos
      - orbit_dir * eff_length
      + core::Vec3f::kAxisY * eff_height;

  // Wall-clip: cast a ray from the target toward the desired position and pull
  // back slightly from any hit to prevent clipping through walls.
  core::Vec3f clip_pos = desired;
  if (physics::PhysicsSystem::IsInstanced()) {
    const core::Vec3f dir    = (desired - target_pos).Normalized();
    const float       dist   = (desired - target_pos).Length();
    constexpr uint16_t kMask = (1u << physics::kLayerWorld);
    const auto hit = physics::PhysicsSystem::Instance().Raycast(
        target_pos, dir, dist, kMask);
    if (hit) {
      clip_pos = hit->position - dir * 0.3f;
    }
  }

  // Spring-lerp toward the clipped position.
  if (!initialized_) {
    position_    = clip_pos;
    initialized_ = true;
  } else {
    const float factor = 1.f - std::exp(-spring_stiffness_ * dt);
    position_ = position_.Lerp(clip_pos, factor);
  }

  // Look-at: always point directly at the target (no smoothing).
  const core::Vec3f look_target = target_pos + core::Vec3f::kAxisY * 1.f;
  const core::Vec3f forward     = (look_target - position_).Normalized();
  const core::Vec3f right       = core::Vec3f::kAxisY.Cross(forward).Normalized();
  const core::Vec3f up          = forward.Cross(right);

  // Build world transform: columns are right, up, -forward, position.
  const core::Mat4f transform(
      right.x,   up.x,  -forward.x,  position_.x,
      right.y,   up.y,  -forward.y,  position_.y,
      right.z,   up.z,  -forward.z,  position_.z,
      0.f,       0.f,    0.f,        1.f);

  camera_->SetWorldTransform(transform);
}

}  // namespace game
