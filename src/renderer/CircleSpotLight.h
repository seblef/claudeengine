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

  // Returns the light-space view-projection matrix for shadow map rendering.
  // view = LookAt(pos, pos + direction, up); proj = PerspectiveRH(2*outer_angle, 1, 0.1, range).
  // Returns proj * view (column-vector convention).
  [[nodiscard]] core::Mat4f GetLightSpaceMatrix() const;

  [[nodiscard]] std::optional<core::Mat4f> ComputeShadowVP() const override;

  [[nodiscard]] float ComputeScreenRadius(const core::Vec3f& eye_pos,
                                          float              half_screen_height,
                                          float              tan_half_fov) const override;

  [[nodiscard]] float GetInnerAngle() const { return inner_angle_; }
  [[nodiscard]] float GetOuterAngle() const { return outer_angle_; }
  [[nodiscard]] float GetRange()      const { return range_; }

  // Returns the world-space direction by rotating local_direction_ with the
  // upper-left 3×3 of the current world matrix.
  [[nodiscard]] core::Vec3f GetDirection() const;

  void SetInnerAngle(float a) { inner_angle_ = a; }
  void SetOuterAngle(float a);
  void SetRange(float r);

  // Sets the light direction from a world-space vector. The local-space
  // direction is derived by applying R^T (the inverse rotation of the current
  // world matrix) so that subsequent world-matrix changes (e.g. gizmo rotation)
  // automatically update the world-space direction.
  void SetDirection(const core::Vec3f& world_dir);

 private:
  void RecomputeLocalBBox();


  // cppcheck-suppress unusedStructMember
  float inner_angle_;
  // cppcheck-suppress unusedStructMember
  float outer_angle_;
  // cppcheck-suppress unusedStructMember
  float range_;
  // Local-space direction (direction with identity world rotation applied).
  // World-space direction = R * local_direction_ where R is the rotation
  // component of the current world matrix.
  // cppcheck-suppress unusedStructMember
  core::Vec3f local_direction_;
};

}  // namespace renderer
