#include "renderer/OmniLight.h"

namespace renderer {

OmniLight::OmniLight(const core::Color& color, float intensity, float radius,
                     const core::Mat4f& world_matrix)
    : Light(color, intensity,
            // Local bbox = sphere AABB centered at origin; world matrix
            // (typically a translation) maps this to world space.
            core::BBox3({-radius, -radius, -radius}, {radius, radius, radius}),
            world_matrix,
            /*always_visible=*/false),
      radius_(radius) {}

LightType OmniLight::GetType() const {
  return LightType::kOmni;
}

core::Mat4f OmniLight::GetVolumeMatrix() const {
  // Extract the translation (world position) from column 3 of the world matrix.
  const core::Mat4f& wm = GetWorldMatrix();
  const core::Vec3f  pos{wm(0, 3), wm(1, 3), wm(2, 3)};
  return core::Mat4f::Translation(pos) *
         core::Mat4f::Scale3D({radius_, radius_, radius_});
}

}  // namespace renderer
