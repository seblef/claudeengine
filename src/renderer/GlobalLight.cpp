#include "renderer/GlobalLight.h"

namespace renderer {

GlobalLight::GlobalLight(const core::Color& color, float intensity,
                         const core::Vec3f& ambient_color,
                         const core::Vec3f& direction)
    : Light(color, intensity,
            core::BBox3::kInfinite,
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
