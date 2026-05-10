#pragma once

#include "core/Color.h"
#include "core/Mat4f.h"
#include "renderer/Light.h"

namespace renderer {

// Point light with spherical influence volume.
//
// GetVolumeMatrix() returns Translation(world_pos) × Scale(radius) — used by
// LightRenderer to draw a unit sphere mesh scaled to match the light's
// influence radius.
class OmniLight : public Light {
 public:
  // world_matrix: typically a pure translation placing the light in the world.
  // radius: radius of the spherical influence volume.
  OmniLight(const core::Color& color, float intensity, float radius,
            const core::Mat4f& world_matrix);

  [[nodiscard]] LightType   GetType()         const override;
  [[nodiscard]] core::Mat4f GetVolumeMatrix() const override;

  [[nodiscard]] float GetRadius() const { return radius_; }
  void SetRadius(float r)               { radius_ = r; }

 private:
  // cppcheck-suppress unusedStructMember
  float radius_;
};

}  // namespace renderer
