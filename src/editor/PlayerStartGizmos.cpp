#include "editor/PlayerStartGizmos.h"

#include "core/Color.h"
#include "core/Mat4f.h"
#include "core/Vec3f.h"
#include "game/GameObjectType.h"
#include "renderer/WireframeRenderer.h"

namespace editor {

namespace {

constexpr float kPoleHeight   = 2.0f;
constexpr float kFlagWidth    = 0.8f;
constexpr float kFlagHeight   = 0.5f;
constexpr float kArrowLength  = 1.5f;
constexpr float kArrowHead    = 0.3f;
constexpr float kArrowSpread  = 0.15f;

const core::Color kPoleColor(1.f, 1.f, 1.f, 1.f);
const core::Color kFlagColor(0.f, 0.8f, 0.267f, 1.f);
const core::Color kFlagColorSelected(0.2f, 1.f, 0.467f, 1.f);

}  // namespace

void EnqueuePlayerStartGizmos(const std::vector<game::GameObject*>& objects,
                               const void* selected_key) {
  renderer::WireframeRenderer& wr = renderer::WireframeRenderer::Instance();

  for (const game::GameObject* obj : objects) {
    if (obj->GetType() != game::GameObjectType::kPlayerStart) continue;

    const core::Mat4f& wt  = obj->GetWorldTransform();
    const core::Vec3f  base(wt(0, 3), wt(1, 3), wt(2, 3));
    const core::Vec3f  top = base + core::Vec3f(0.f, kPoleHeight, 0.f);

    wr.PushOverlaySegment(base, top, kPoleColor);

    const core::Vec3f flag_a = top;
    const core::Vec3f flag_b = top + core::Vec3f(kFlagWidth, 0.f, 0.f);
    const core::Vec3f flag_c = top + core::Vec3f(0.f, -kFlagHeight, 0.f);

    const core::Color& flag_color =
        wr.IsHighlighted(selected_key == obj ? obj : nullptr)
            ? kFlagColorSelected
            : kFlagColor;

    wr.PushOverlaySegment(flag_a, flag_b, flag_color);
    wr.PushOverlaySegment(flag_b, flag_c, flag_color);
    wr.PushOverlaySegment(flag_c, flag_a, flag_color);

    // Direction arrow along local +Z (column 2 of the world transform).
    const core::Vec3f dir(wt(0, 2), wt(1, 2), wt(2, 2));
    const core::Vec3f perp(wt(0, 0), wt(1, 0), wt(2, 0));
    const core::Vec3f tip = base + dir * kArrowLength;
    wr.PushOverlaySegment(base, tip, flag_color);
    wr.PushOverlaySegment(tip, tip - dir * kArrowHead + perp * kArrowSpread, flag_color);
    wr.PushOverlaySegment(tip, tip - dir * kArrowHead - perp * kArrowSpread, flag_color);
  }
}

}  // namespace editor
