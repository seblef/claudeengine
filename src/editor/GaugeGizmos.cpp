#include "editor/GaugeGizmos.h"

#include "core/Color.h"
#include "core/Mat4f.h"
#include "core/Vec3f.h"
#include "game/GameObjectType.h"
#include "renderer/WireframeRenderer.h"

namespace editor {

namespace {

const core::Color kGaugeColor(1.f, 0.863f, 0.196f, 0.78f);          // yellow-gold
const core::Color kGaugeColorSelected(1.f, 0.97f, 0.4f, 1.f);       // brighter when selected

const core::Vec3f kHalfExtents(0.5f, 0.5f, 0.5f);

}  // namespace

void EnqueueGaugeGizmos(const std::vector<game::GameObject*>& objects,
                         const void* selected_key) {
  renderer::WireframeRenderer& wr = renderer::WireframeRenderer::Instance();

  for (const game::GameObject* obj : objects) {
    if (obj->GetType() != game::GameObjectType::kGauge) continue;

    const bool selected = (selected_key == obj);
    const core::Color& color = selected ? kGaugeColorSelected : kGaugeColor;
    wr.PushBox(kHalfExtents, color, obj->GetWorldTransform());
  }
}

}  // namespace editor
