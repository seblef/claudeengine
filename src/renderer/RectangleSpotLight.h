#pragma once

#include "core/Color.h"
#include "core/Mat4f.h"
#include "core/Vec3f.h"
#include "renderer/Light.h"

namespace renderer {

// Rectangular beam spot light with independent horizontal and vertical
// half-angle falloff.
//
// GetVolumeMatrix() returns a transform scaling a unit pyramid (apex at origin,
// base 1×1 at z=1) to match the light's influence frustum.
class RectangleSpotLight : public Light {
 public:
  // h_angle: horizontal half-angle (radians).
  // v_angle: vertical half-angle (radians).
  // range: maximum influence distance.
  // direction: normalized world-space direction the light points toward.
  // world_matrix: typically a pure translation (position).
  RectangleSpotLight(const core::Color& color, float intensity,
                     float h_angle, float v_angle, float range,
                     const core::Vec3f& direction,
                     const core::Mat4f& world_matrix);

  [[nodiscard]] core::Mat4f GetVolumeMatrix() const override;

  // Returns the light-space view-projection matrix for shadow map rendering.
  // view = LookAt(pos, pos + direction, up);
  // proj = PerspectiveRH(2*v_angle, tan(h_angle)/tan(v_angle), 0.1, range).
  // Returns proj * view (column-vector convention).
  [[nodiscard]] core::Mat4f GetLightSpaceMatrix() const;

  [[nodiscard]] std::optional<core::Mat4f> ComputeShadowVP() const override;

  [[nodiscard]] float ComputeScreenRadius(const core::Vec3f& eye_pos,
                                          float              half_screen_height,
                                          float              tan_half_fov) const override;

  [[nodiscard]] float GetHAngle()                  const { return h_angle_; }
  [[nodiscard]] float GetVAngle()                  const { return v_angle_; }
  [[nodiscard]] float GetRange()                   const { return range_; }
  [[nodiscard]] const core::Vec3f& GetDirection()  const { return direction_; }

  void SetHAngle(float a)                 { h_angle_ = a; }
  void SetVAngle(float a)                 { v_angle_ = a; }
  void SetRange(float r)                  { range_ = r; }
  void SetDirection(const core::Vec3f& d) { direction_ = d; }

 private:
  // cppcheck-suppress unusedStructMember
  float h_angle_;
  // cppcheck-suppress unusedStructMember
  float v_angle_;
  // cppcheck-suppress unusedStructMember
  float range_;
  // cppcheck-suppress unusedStructMember
  core::Vec3f direction_;
};

}  // namespace renderer
