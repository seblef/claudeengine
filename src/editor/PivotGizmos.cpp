#include "editor/PivotGizmos.h"

#include "core/Color.h"
#include "core/Mat4f.h"
#include "core/Vec3f.h"
#include "game/GameObjectType.h"
#include "renderer/WireframeRenderer.h"

namespace editor {

namespace {

constexpr float kArmLength = 0.5f;

const core::Color kColorX(1.f, 0.2f, 0.2f, 1.f);  // red
const core::Color kColorY(0.2f, 1.f, 0.2f, 1.f);  // green
const core::Color kColorZ(0.2f, 0.4f, 1.f, 1.f);  // blue
const core::Color kColorSelected(1.f, 1.f, 0.2f, 1.f);

}  // namespace

void EnqueuePivotGizmos(const std::vector<game::GameObject*>& objects,
                         const void* selected_key) {
  renderer::WireframeRenderer& wr = renderer::WireframeRenderer::Instance();

  for (const game::GameObject* obj : objects) {
    if (obj->GetType() != game::GameObjectType::kPivot) continue;

    const core::Mat4f& wt = obj->GetWorldTransform();
    const core::Vec3f  origin(wt(0, 3), wt(1, 3), wt(2, 3));

    // Local axes: columns 0/1/2 of the rotation-scale block.
    const core::Vec3f axis_x(wt(0, 0), wt(1, 0), wt(2, 0));
    const core::Vec3f axis_y(wt(0, 1), wt(1, 1), wt(2, 1));
    const core::Vec3f axis_z(wt(0, 2), wt(1, 2), wt(2, 2));

    const bool selected = (selected_key == obj);
    if (selected) {
      // Draw all three arms in highlight colour when selected.
      wr.PushOverlaySegment(origin, origin + axis_x * kArmLength, kColorSelected);
      wr.PushOverlaySegment(origin, origin + axis_y * kArmLength, kColorSelected);
      wr.PushOverlaySegment(origin, origin + axis_z * kArmLength, kColorSelected);
    } else {
      wr.PushOverlaySegment(origin, origin + axis_x * kArmLength, kColorX);
      wr.PushOverlaySegment(origin, origin + axis_y * kArmLength, kColorY);
      wr.PushOverlaySegment(origin, origin + axis_z * kArmLength, kColorZ);
    }
  }
}

}  // namespace editor
