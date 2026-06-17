#include "game/FPSCameraController.h"

#include <algorithm>
#include <cmath>

#include "core/EventType.h"
#include "core/Key.h"
#include "core/Mat4f.h"
#include "game/GameCamera.h"
#include "physics/CharacterController.h"
#include "physics/PhysicsSystem.h"

namespace game {

FPSCameraController::FPSCameraController() : position_(core::Vec3f::kZero) {}

FPSCameraController::~FPSCameraController() {
  character_.reset();
}

void FPSCameraController::SetPosition(core::Vec3f pos) {
  position_ = pos;
}

void FPSCameraController::SetCamera(GameCamera* camera) {
  camera_ = camera;
  // CharacterController creation is deferred to the first Update() so that
  // SetPosition() can be called after SetCamera() without spawning the capsule
  // at the wrong location.
}

void FPSCameraController::OnEvent(const core::Event& event) {
  switch (event.type) {
    case core::EventType::kKeyDown:
      if (event.key == core::Key::kUp)    k_forward_ = true;
      if (event.key == core::Key::kDown)  k_back_    = true;
      if (event.key == core::Key::kLeft)  k_left_    = true;
      if (event.key == core::Key::kRight) k_right_   = true;
      if (event.key == core::Key::kA)     k_up_      = true;
      if (event.key == core::Key::kZ)     k_down_    = true;
      if (event.key == core::Key::kSpace) k_jump_    = true;
      break;
    case core::EventType::kKeyUp:
      if (event.key == core::Key::kUp)    k_forward_ = false;
      if (event.key == core::Key::kDown)  k_back_    = false;
      if (event.key == core::Key::kLeft)  k_left_    = false;
      if (event.key == core::Key::kRight) k_right_   = false;
      if (event.key == core::Key::kA)     k_up_      = false;
      if (event.key == core::Key::kZ)     k_down_    = false;
      if (event.key == core::Key::kSpace) k_jump_    = false;
      break;
    case core::EventType::kMouseMoved:
      if (prev_mouse_x_ >= 0.f) {
        yaw_   += (event.mouse_x - prev_mouse_x_) * kMouseSensitivity;
        pitch_ += (event.mouse_y - prev_mouse_y_) * kMouseSensitivity;
        pitch_  = std::max(-1.5f, std::min(1.5f, pitch_));
      }
      prev_mouse_x_ = event.mouse_x;
      prev_mouse_y_ = event.mouse_y;
      break;
    case core::EventType::kWindowLostFocus:
      // Reset tracking so the first move after re-focus doesn't jump.
      prev_mouse_x_ = -1.f;
      prev_mouse_y_ = -1.f;
      break;
    default:
      break;
  }
}

void FPSCameraController::Update(float dt) {
  if (!camera_) return;

  // Lazy-init: create the capsule on the first tick, after SetPosition() has
  // had a chance to run. Checked every frame in case PhysicsSystem is created
  // after the controller (unlikely in practice but safe).
  if (!character_ && physics::PhysicsSystem::IsInstanced()) {
    const core::Mat4f initial_transform(
        1.f, 0.f, 0.f, position_.x,
        0.f, 1.f, 0.f, position_.y,
        0.f, 0.f, 1.f, position_.z,
        0.f, 0.f, 0.f, 1.f);
    character_ = physics::PhysicsSystem::Instance().CreateCharacter(
        kCapsuleRadius, kCapsuleHalfHeight, initial_transform);
  }

  const core::Vec3f look = {
       std::sin(yaw_) * std::cos(pitch_),
      -std::sin(pitch_),
      -std::cos(yaw_) * std::cos(pitch_)
  };
  const core::Vec3f right = look.Cross(core::Vec3f::kAxisY).Normalized();

  // Horizontal velocity components shared by both physics and free-fly paths.
  const float spd = kMoveSpeed;
  core::Vec3f hvel;
  if (k_forward_) hvel += look  * spd;
  if (k_back_)    hvel -= look  * spd;
  if (k_right_)   hvel += right * spd;
  if (k_left_)    hvel -= right * spd;
  // Suppress vertical components from look (keep movement flat).
  hvel.y = 0.f;

  core::Vec3f position;

  if (character_) {
    // --- Physics-driven path ---
    const bool grounded = character_->IsGrounded();
    if (grounded) {
      if (k_jump_) {
        vel_y_ = kJumpSpeed;
      } else {
        vel_y_ = 0.f;
      }
    } else {
      vel_y_ -= kGravity * dt;
    }

    const core::Vec3f velocity(hvel.x, vel_y_, hvel.z);
    character_->Move(velocity, dt);

    // Capsule base position + eye offset.
    const core::Mat4f capsule_transform = character_->GetWorldTransform();
    position = core::Vec3f(capsule_transform(0, 3),
                           capsule_transform(1, 3),
                           capsule_transform(2, 3));
    position.y += kCapsuleHalfHeight + kEyeOffset;
  } else {
    // --- Free-fly fallback (no PhysicsSystem) ---
    position_ += hvel * dt;
    if (k_up_)   position_ += core::Vec3f::kAxisY * (kMoveSpeed * dt);
    if (k_down_) position_ -= core::Vec3f::kAxisY * (kMoveSpeed * dt);
    position = position_;
  }

  // Build world transform: columns are right, world-up, -look, position.
  const core::Mat4f transform(
      right.x,  core::Vec3f::kAxisY.x,  -look.x,  position.x,
      right.y,  core::Vec3f::kAxisY.y,  -look.y,  position.y,
      right.z,  core::Vec3f::kAxisY.z,  -look.z,  position.z,
      0.f,      0.f,                     0.f,      1.f);

  camera_->SetWorldTransform(transform);
}

}  // namespace game
