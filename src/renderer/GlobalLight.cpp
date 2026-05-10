#include "renderer/GlobalLight.h"

#include <limits>

namespace renderer {

GlobalLight::GlobalLight(const core::Color& color, float intensity,
                         const core::Vec3f& ambient_color,
                         const core::Vec3f& direction)
    : Light(color, intensity,
            // Infinite AABB: always-visible flag bypasses the AABB check, but
            // provide a huge finite box to avoid NaN in any transform math.
            core::BBox3({-1e30f, -1e30f, -1e30f}, {1e30f, 1e30f, 1e30f}),
            core::Mat4f::kIdentity,
            /*always_visible=*/true),
      ambient_color_(ambient_color),
      direction_(direction) {}

LightType GlobalLight::GetType() const {
  return LightType::kGlobal;
}

core::Mat4f GlobalLight::GetVolumeMatrix() const {
  return core::Mat4f::kIdentity;
}

}  // namespace renderer
