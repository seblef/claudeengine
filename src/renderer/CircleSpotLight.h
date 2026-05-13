#pragma once

#include "core/Color.h"
#include "core/Mat4f.h"
#include "core/Vec3f.h"
#include "renderer/Light.h"

namespace renderer {

// Circular (cone) spot light with inner/outer angle falloff.
//
// GetVolumeMatrix() returns a transform that scales a unit cone so its apex
// sits at the light position and its base disk covers the maximum influence
// area at distance `range`.
class CircleSpotLight : public Light {
 public:
  // inner_angle / outer_angle: half-angles (radians) controlling the
  // intensity falloff from the cone center to its edge.
  // range: maximum influence distance.
  // direction: normalized world-space direction the light points toward.
  // world_matrix: typically a pure translation (position).
  CircleSpotLight(const core::Color& color, float intensity,
                  float inner_angle, float outer_angle, float range,
                  const core::Vec3f& direction,
                  const core::Mat4f& world_matrix);

  [[nodiscard]] core::Mat4f GetVolumeMatrix() const override;

  // Computes the light-space VP matrix for shadow map rendering.
  // FOV = 2 * outer_angle, aspect = 1, near = 0.1, far = range.
  [[nodiscard]] std::optional<core::Mat4f> ComputeShadowVP() const override;

  [[nodiscard]] float GetInnerAngle()              const { return inner_angle_; }
  [[nodiscard]] float GetOuterAngle()              const { return outer_angle_; }
  [[nodiscard]] float GetRange()                   const { return range_; }
  [[nodiscard]] const core::Vec3f& GetDirection()  const { return direction_; }

  void SetInnerAngle(float a)             { inner_angle_ = a; }
  void SetOuterAngle(float a)             { outer_angle_ = a; }
  void SetRange(float r)                  { range_ = r; }
  void SetDirection(const core::Vec3f& d) { direction_ = d; }

 private:
  // cppcheck-suppress unusedStructMember
  float inner_angle_;
  // cppcheck-suppress unusedStructMember
  float outer_angle_;
  // cppcheck-suppress unusedStructMember
  float range_;
  // cppcheck-suppress unusedStructMember
  core::Vec3f direction_;
};

}  // namespace renderer
