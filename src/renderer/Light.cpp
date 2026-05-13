#include "renderer/Light.h"

#include "renderer/LightRenderer.h"

namespace renderer {

Light::Light(LightType type, const core::Color& color, float intensity,
             const core::BBox3& local_bbox,
             const core::Mat4f& world_matrix,
             bool always_visible)
    : Renderable(local_bbox, world_matrix, always_visible),
      type_(type),
      color_(color),
      intensity_(intensity) {}

void Light::Enqueue() {
  LightRenderer::Instance().AddLight(this);
}

// static
float Light::ScreenRadius(const core::Vec3f& sphere_center,
                           float              sphere_radius,
                           const core::Vec3f& eye_pos,
                           float              half_screen_height,
                           float              tan_half_fov) {
  const float dist = (sphere_center - eye_pos).Length();
  if (dist < 1e-4f) return half_screen_height * 2.f;  // camera inside sphere
  return sphere_radius * half_screen_height / (tan_half_fov * dist);
}

}  // namespace renderer
