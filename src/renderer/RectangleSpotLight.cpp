#include "renderer/RectangleSpotLight.h"

#include <cmath>
#include <optional>

namespace renderer {

namespace {
// Returns a rotation matrix mapping the +Z axis to `dir.Normalized()`.
core::Mat4f AlignZToDir(const core::Vec3f& dir) {
  const core::Vec3f fwd = dir.Normalized();
  core::Vec3f ref = {0.f, 1.f, 0.f};
  if (std::fabs(fwd.y) > 0.99f) ref = {1.f, 0.f, 0.f};
  const core::Vec3f right = ref.Cross(fwd).Normalized();
  const core::Vec3f up    = fwd.Cross(right);
  return {right.x, up.x, fwd.x, 0.f,
          right.y, up.y, fwd.y, 0.f,
          right.z, up.z, fwd.z, 0.f,
          0.f,     0.f,  0.f,   1.f};
}
}  // namespace

RectangleSpotLight::RectangleSpotLight(const core::Color& color, float intensity,
                                       float h_angle, float v_angle, float range,
                                       const core::Vec3f& direction,
                                       const core::Mat4f& world_matrix)
    : Light(LightType::kRectSpot, color, intensity,
            core::BBox3({-range, -range, -range}, {range, range, range}),
            world_matrix,
            /*always_visible=*/false),
      h_angle_(h_angle),
      v_angle_(v_angle),
      range_(range),
      direction_(direction) {}

core::Mat4f RectangleSpotLight::GetVolumeMatrix() const {
  const core::Mat4f& wm  = GetWorldMatrix();
  const core::Vec3f  pos = {wm(0, 3), wm(1, 3), wm(2, 3)};
  // Scale the unit pyramid's base half-extents to the frustum dimensions.
  return core::Mat4f::Translation(pos) *
         AlignZToDir(direction_) *
         core::Mat4f::Scale3D({range_ * std::tan(h_angle_),
                               range_ * std::tan(v_angle_),
                               range_});
}

float RectangleSpotLight::ComputeScreenRadius(const core::Vec3f& eye_pos,
                                               float              half_screen_height,
                                               float              tan_half_fov) const {
  const core::Mat4f& wm       = GetWorldMatrix();
  const core::Vec3f  pos{wm(0, 3), wm(1, 3), wm(2, 3)};
  const float        angle    = std::max(h_angle_, v_angle_);
  const float        cos_a    = std::cos(angle);
  const float        sphere_r = (cos_a > 1e-6f) ? range_ / (2.f * cos_a) : range_;
  const core::Vec3f  center   = pos + direction_ * (range_ * 0.5f);
  return ScreenRadius(center, sphere_r, eye_pos, half_screen_height, tan_half_fov);
}

core::Mat4f RectangleSpotLight::GetLightSpaceMatrix() const {
  const float        kNear = range_ * 0.05f;
  const core::Mat4f& wm = GetWorldMatrix();
  const core::Vec3f  pos(wm(0, 3), wm(1, 3), wm(2, 3));
  const core::Vec3f  up = (std::abs(direction_.y) > 0.9f)
                              ? core::Vec3f(1.f, 0.f, 0.f)
                              : core::Vec3f(0.f, 1.f, 0.f);
  const core::Mat4f view = core::Mat4f::LookAtRH(pos, pos + direction_, up);
  // aspect = tan(h_angle) / tan(v_angle) maps the rectangular frustum correctly.
  const float aspect = std::tan(h_angle_) / std::tan(v_angle_);
  const core::Mat4f proj =
      core::Mat4f::PerspectiveRH(2.f * v_angle_, aspect, kNear, range_);
  return proj * view;
}

std::optional<core::Mat4f> RectangleSpotLight::ComputeShadowVP() const {
  return GetLightSpaceMatrix();
}

}  // namespace renderer
