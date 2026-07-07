#include "vfx/ScreenShake.h"

#include <algorithm>

#include "core/Mat4f.h"
#include "game/GameCamera.h"
#include "game/GameSystem.h"
#include "game/ICameraController.h"

namespace vfx {

void ScreenShake::Play(const core::Vec3f& world_pos, const core::Vec3f& /*direction*/) {
  finished_ = true;
  elapsed_  = 0.f;

  if (!game::GameSystem::IsInstanced()) return;
  const game::GameSystem& game = game::GameSystem::Instance();

  game::ICameraController* controller = game.GetCameraController();
  const game::GameCamera*  camera     = game.GetActiveCamera();
  if (!controller || !camera) return;

  const core::Mat4f& cam_transform = camera->GetWorldTransform();
  const core::Vec3f  cam_pos = {cam_transform(0, 3), cam_transform(1, 3), cam_transform(2, 3)};
  const float distance  = std::max((world_pos - cam_pos).Length(), 1.f);
  const float magnitude = base_magnitude_ / distance;

  controller->ApplyShake(magnitude, duration_);
  finished_ = false;
}

void ScreenShake::Update(float dt) {
  if (finished_) return;
  elapsed_ += dt;
  if (elapsed_ >= duration_) finished_ = true;
}

}  // namespace vfx
