#include "renderer/CircleSpotLight.h"

#include <cmath>

namespace renderer {

namespace {
// Returns a rotation matrix mapping the +Z axis to `dir.Normalized()`.
// Used to orient the cone volume along the light direction.
core::Mat4f AlignZToDir(const core::Vec3f& dir) {
  const core::Vec3f fwd = dir.Normalized();
  // Choose a reference up vector that is not parallel to fwd.
  core::Vec3f ref = {0.f, 1.f, 0.f};
  if (std::fabs(fwd.y) > 0.99f) ref = {1.f, 0.f, 0.f};
  const core::Vec3f right = ref.Cross(fwd).Normalized();
  const core::Vec3f up    = fwd.Cross(right);
  // Columns: right, up, fwd (rotation from local XYZ to world space).
  return {right.x, up.x, fwd.x, 0.f,
          right.y, up.y, fwd.y, 0.f,
          right.z, up.z, fwd.z, 0.f,
          0.f,     0.f,  0.f,   1.f};
}
}  // namespace

CircleSpotLight::CircleSpotLight(const core::Color& color, float intensity,
                                 float inner_angle, float outer_angle,
                                 float range, const core::Vec3f& direction,
                                 const core::Mat4f& world_matrix)
    : Light(color, intensity,
            // Conservative sphere AABB of radius `range` in local space.
            core::BBox3({-range, -range, -range}, {range, range, range}),
            world_matrix,
            /*always_visible=*/false),
      inner_angle_(inner_angle),
      outer_angle_(outer_angle),
      range_(range),
      direction_(direction) {}

LightType CircleSpotLight::GetType() const {
  return LightType::kCircleSpot;
}

core::Mat4f CircleSpotLight::GetVolumeMatrix() const {
  const core::Mat4f& wm  = GetWorldMatrix();
  const core::Vec3f  pos = {wm(0, 3), wm(1, 3), wm(2, 3)};
  // Base radius = range * tan(outer_angle); height = range.
  const float r = range_ * std::tan(outer_angle_);
  return core::Mat4f::Translation(pos) *
         AlignZToDir(direction_) *
         core::Mat4f::Scale3D({r, r, range_});
}

}  // namespace renderer
