#include "game/PlayerVehicleController.h"

#include <algorithm>

#include "core/EventType.h"
#include "core/Key.h"

namespace game {

void PlayerVehicleController::OnEvent(const core::Event& event) {
  switch (event.type) {
    case core::EventType::kKeyDown:
      if (event.key == core::Key::kW || event.key == core::Key::kUp)
        k_throttle_ = true;
      if (event.key == core::Key::kS || event.key == core::Key::kDown)
        k_brake_ = true;
      if (event.key == core::Key::kA || event.key == core::Key::kLeft)
        k_steer_left_ = true;
      if (event.key == core::Key::kD || event.key == core::Key::kRight)
        k_steer_right_ = true;
      if (event.key == core::Key::kSpace)
        k_handbrake_ = true;
      break;
    case core::EventType::kKeyUp:
      if (event.key == core::Key::kW || event.key == core::Key::kUp)
        k_throttle_ = false;
      if (event.key == core::Key::kS || event.key == core::Key::kDown)
        k_brake_ = false;
      if (event.key == core::Key::kA || event.key == core::Key::kLeft)
        k_steer_left_ = false;
      if (event.key == core::Key::kD || event.key == core::Key::kRight)
        k_steer_right_ = false;
      if (event.key == core::Key::kSpace)
        k_handbrake_ = false;
      break;
    default:
      break;
  }
}

void PlayerVehicleController::Update(float dt) {
  // Throttle takes priority; both keys held → full throttle, no brake.
  throttle_ = k_throttle_ ? 1.0f : 0.0f;
  brake_    = (!k_throttle_ && k_brake_) ? 1.0f : 0.0f;

  // Steer ramp: advance toward target, return to 0 when no key held.
  const float steer_target =
      k_steer_right_ ? 1.0f : (k_steer_left_ ? -1.0f : 0.0f);

  if (k_steer_left_ || k_steer_right_) {
    // Ramp toward target direction.
    const float delta = kSteerSpeed * dt;
    if (steer_ < steer_target)
      steer_ = std::min(steer_ + delta, steer_target);
    else
      steer_ = std::max(steer_ - delta, steer_target);
  } else {
    // Return to centre.
    const float delta = kSteerReturnSpeed * dt;
    if (steer_ > 0.f)
      steer_ = std::max(steer_ - delta, 0.f);
    else
      steer_ = std::min(steer_ + delta, 0.f);
  }
}

}  // namespace game
