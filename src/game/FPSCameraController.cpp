#include "game/FPSCameraController.h"

#include <algorithm>
#include <cmath>

#include "core/EventType.h"
#include "core/Key.h"
#include "core/Mat4f.h"
#include "game/GameCamera.h"
#include "terrain/TerrainData.h"

namespace game {

FPSCameraController::FPSCameraController() : position_(core::Vec3f::kZero) {}

void FPSCameraController::SetPosition(core::Vec3f pos) {
  position_ = pos;
}

void FPSCameraController::SetCamera(GameCamera* camera) {
  camera_ = camera;
}

void FPSCameraController::SetTerrain(const terrain::TerrainData* terrain) {
  terrain_ = terrain;
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
    default:
      break;
  }
}

void FPSCameraController::Update(float dt) {
  if (!camera_) return;

  const core::Vec3f look = {
       std::sin(yaw_) * std::cos(pitch_),
      -std::sin(pitch_),
      -std::cos(yaw_) * std::cos(pitch_)
  };
  const core::Vec3f right = core::Vec3f::kAxisY.Cross(look).Normalized();

  const float spd = kMoveSpeed * dt;
  if (k_forward_) position_ += look                * spd;
  if (k_back_)    position_ -= look                * spd;
  if (k_right_)   position_ += right               * spd;
  if (k_left_)    position_ -= right               * spd;
  if (!terrain_) {
    if (k_up_)   position_ += core::Vec3f::kAxisY * spd;
    if (k_down_) position_ -= core::Vec3f::kAxisY * spd;
  }

  if (k_jump_ && grounded_) {
    vel_y_    = kJumpSpeed;
    grounded_ = false;
  }

  position_.y += vel_y_ * dt;
  vel_y_      -= kGravity * dt;

  if (terrain_) {
    const float ground = terrain_->GetHeight(position_.x, position_.z) + kPlayerHeight;
    if (position_.y <= ground) {
      position_.y = ground;
      vel_y_      = 0.f;
      grounded_   = true;
    }
  } else {
    grounded_ = true;
  }

  // Build world transform: columns are right, world-up, -look, position.
  const core::Mat4f transform(
      right.x,  core::Vec3f::kAxisY.x,  -look.x,  position_.x,
      right.y,  core::Vec3f::kAxisY.y,  -look.y,  position_.y,
      right.z,  core::Vec3f::kAxisY.z,  -look.z,  position_.z,
      0.f,      0.f,                     0.f,      1.f);

  camera_->SetWorldTransform(transform);
}

}  // namespace game
