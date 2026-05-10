#pragma once

#include "core/Color.h"
#include "core/Mat4f.h"
#include "core/Vec3f.h"
#include "renderer/Light.h"

namespace renderer {

// Directional light with an ambient contribution; infinite influence.
//
// Always visible — the AABB test is bypassed. GetVolumeMatrix() returns the
// identity matrix; LightRenderer draws a fullscreen quad so every pixel on
// screen participates in the directional Blinn-Phong computation.
class GlobalLight : public Light {
 public:
  // ambient_color: additive per-channel ambient term applied to all surfaces.
  // direction: world-space unit vector pointing toward the light source
  //            (i.e. from surface toward the sun — not the ray direction).
  GlobalLight(const core::Color& color, float intensity,
              const core::Vec3f& ambient_color, const core::Vec3f& direction);

  [[nodiscard]] core::Mat4f GetVolumeMatrix() const override;

  [[nodiscard]] const core::Vec3f& GetAmbientColor() const { return ambient_color_; }
  [[nodiscard]] const core::Vec3f& GetDirection()    const { return direction_; }

  void SetAmbientColor(const core::Vec3f& c) { ambient_color_ = c; }
  void SetDirection(const core::Vec3f& d)    { direction_ = d; }

 private:
  // cppcheck-suppress unusedStructMember
  core::Vec3f ambient_color_;
  // cppcheck-suppress unusedStructMember
  core::Vec3f direction_;
};

}  // namespace renderer
