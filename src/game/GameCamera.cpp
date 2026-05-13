#include "game/GameCamera.h"

#include "core/BBox3.h"

namespace game {

GameCamera::GameCamera(core::ProjectionType proj_type,
                       core::CoordinateSystem coord_system)
    : GameObject(GameObjectType::kCamera, core::BBox3{}),
      camera_(proj_type, coord_system) {
  camera_.SetFOV(1.0472f);   // 60 degrees
  camera_.SetMinDepth(0.1f);
  camera_.SetMaxDepth(1000.f);
  camera_.UpdateMatrices();
}

void GameCamera::OnWorldTransformUpdated() {
  const core::Mat4f& m = GetWorldTransform();
  const core::Vec3f pos     = {m(0, 3),  m(1, 3),  m(2, 3)};
  const core::Vec3f forward = {-m(0, 2), -m(1, 2), -m(2, 2)};
  const core::Vec3f up      = {m(0, 1),  m(1, 1),  m(2, 1)};
  camera_.SetPosition(pos);
  camera_.SetTarget(pos + forward);
  camera_.SetUp(up);
  camera_.UpdateMatrices();
}

void GameCamera::SetFOV(float fov_radians) {
  camera_.SetFOV(fov_radians);
  camera_.UpdateMatrices();
}

void GameCamera::SetScreenCenter(core::Vec2f center) {
  camera_.SetScreenCenter(center);
  camera_.UpdateMatrices();
}

void GameCamera::SetMinDepth(float min_depth) {
  camera_.SetMinDepth(min_depth);
  camera_.UpdateMatrices();
}

void GameCamera::SetMaxDepth(float max_depth) {
  camera_.SetMaxDepth(max_depth);
  camera_.UpdateMatrices();
}

const core::Camera* GameCamera::GetCamera() const { return &camera_; }

}  // namespace game
