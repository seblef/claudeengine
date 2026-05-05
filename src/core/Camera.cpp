#include "core/Camera.h"

namespace core {

Camera::Camera(ProjectionType proj_type, CoordinateSystem coord_system)
    : proj_type_(proj_type), coord_system_(coord_system) {}

void Camera::SetPosition(Vec3f position)  { position_      = position;  }
void Camera::SetTarget(Vec3f target)      { target_        = target;    }
void Camera::SetMinDepth(float min_depth) { min_depth_     = min_depth; }
void Camera::SetMaxDepth(float max_depth) { max_depth_     = max_depth; }
void Camera::SetScreenCenter(Vec2f sc)    { screen_center_ = sc;        }
void Camera::SetFOV(float fov_y)          { fov_           = fov_y;     }
void Camera::SetUp(Vec3f up)              { up_            = up;        }

void Camera::UpdateMatrices() {
  view_ = (coord_system_ == CoordinateSystem::kRightHanded)
      ? Mat4f::LookAtRH(position_, target_, up_)
      : Mat4f::LookAtLH(position_, target_, up_);

  // View matrix rows 0 and 1 are the camera's right and up axes in world space.
  x_axis_ = {view_(0, 0), view_(0, 1), view_(0, 2)};
  y_axis_ = {view_(1, 0), view_(1, 1), view_(1, 2)};

  inv_view_ = view_.Inverse();

  const float aspect = (screen_center_.y > 0.f)
      ? (screen_center_.x / screen_center_.y) : 1.f;

  if (proj_type_ == ProjectionType::kPerspective) {
    proj_ = (coord_system_ == CoordinateSystem::kRightHanded)
        ? Mat4f::PerspectiveRH(fov_, aspect, min_depth_, max_depth_)
        : Mat4f::PerspectiveLH(fov_, aspect, min_depth_, max_depth_);
  } else {
    const float w = screen_center_.x * 2.f;
    const float h = screen_center_.y * 2.f;
    proj_ = (coord_system_ == CoordinateSystem::kRightHanded)
        ? Mat4f::OrthoRH(w, h, min_depth_, max_depth_)
        : Mat4f::OrthoLH(w, h, min_depth_, max_depth_);
  }

  // Column-vector convention: clip = proj * view * v  →  view_proj = proj * view.
  view_proj_ = proj_ * view_;
}

}  // namespace core
