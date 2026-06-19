#include "editor/SoundEmitterGizmos.h"

#include <algorithm>
#include <cmath>

#include "core/Color.h"
#include "core/Mat4f.h"
#include "core/Vec3f.h"
#include "game/GameObjectType.h"
#include "game/GameSoundEmitter.h"
#include "renderer/WireframeRenderer.h"

namespace editor {

namespace {

// Teal gizmo colour, consistent with light gizmo style.
const core::Color kTeal(0.f, 0.75f, 0.75f, 1.f);
const core::Color kTealSelected(0.2f, 1.f, 1.f, 1.f);

// Fraction of the camera orbit distance used as the minimum sphere radius so
// that gizmos remain visible when the camera is far from the emitter.
constexpr float kMinGizmoScale = 0.04f;

}  // namespace

void EnqueueSoundEmitterGizmos(const std::vector<game::GameObject*>& objects,
                                const void* selected_key,
                                float camera_distance) {
  renderer::WireframeRenderer& wr = renderer::WireframeRenderer::Instance();
  const float min_vis = camera_distance * kMinGizmoScale;

  for (const game::GameObject* obj : objects) {
    if (obj->GetType() != game::GameObjectType::kSoundEmitter) continue;

    const auto* emitter = static_cast<const game::GameSoundEmitter*>(obj);

    const core::Mat4f& wt  = emitter->GetWorldTransform();
    const core::Vec3f  pos(wt(0, 3), wt(1, 3), wt(2, 3));
    const core::Mat4f  t   = core::Mat4f::Translation(pos);

    const core::Color& color = wr.IsHighlighted(selected_key == obj ? obj : nullptr)
                               ? kTealSelected : kTeal;

    const float r_inner = std::max(emitter->GetMinDistance(), min_vis);
    const float r_outer = std::max(emitter->GetMaxDistance(),
                                   std::max(r_inner * 1.5f, 2.f * min_vis));

    wr.PushOverlaySphere(r_inner, color, t);
    wr.PushOverlaySphere(r_outer, color, t);
  }
}

}  // namespace editor
