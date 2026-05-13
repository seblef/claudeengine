#include "renderer/NoCullingVisibilitySystem.h"

#include "renderer/Renderable.h"

namespace renderer {

NoCullingVisibilitySystem::NoCullingVisibilitySystem(
    const core::BBox3& world_bounds)
    : IVisibilitySystem(world_bounds) {}

NoCullingVisibilitySystem::~NoCullingVisibilitySystem() {
  for (Renderable* r : renderables_) r->SetVisibilitySystem(nullptr);
}

void NoCullingVisibilitySystem::AddRenderable(Renderable* r) {
  r->SetVisibilityId(renderables_.size());
  renderables_.push_back(r);
  r->SetVisibilitySystem(this);
}

void NoCullingVisibilitySystem::RemoveRenderable(Renderable* r) {
  const uintptr_t idx  = r->GetVisibilityId();
  const uintptr_t last = renderables_.size() - 1;
  if (idx != last) {
    renderables_[idx] = renderables_[last];
    renderables_[idx]->SetVisibilityId(idx);
  }
  renderables_.pop_back();
  r->SetVisibilitySystem(nullptr);
}

void NoCullingVisibilitySystem::OnRenderableMoved(Renderable* /*r*/) {}

void NoCullingVisibilitySystem::CullAndEnqueue(
    const core::ViewFrustum& /*frustum*/) {
  for (Renderable* r : renderables_) r->Enqueue();
}

void NoCullingVisibilitySystem::CullAndCollect(
    const core::ViewFrustum& /*frustum*/,
    std::vector<Renderable*>& out) const {
  out.insert(out.end(), renderables_.begin(), renderables_.end());
}

void NoCullingVisibilitySystem::Clear() {
  for (Renderable* r : renderables_) r->SetVisibilitySystem(nullptr);
  renderables_.clear();
}

}  // namespace renderer
