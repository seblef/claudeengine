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

}  // namespace renderer
