#include "renderer/OctreeVisibilitySystem.h"

#include <algorithm>
#include <cmath>

#include "core/ViewFrustum.h"
#include "renderer/Renderable.h"

namespace renderer {

OctreeVisibilitySystem::OctreeVisibilitySystem(const core::BBox3& world_bounds,
                                               int max_depth)
    : IVisibilitySystem(world_bounds), max_depth_(max_depth) {
  root_.bounds = world_bounds;
}

int OctreeVisibilitySystem::ComputeNumLevels(float world_size,
                                             float min_cell_size) {
  const int levels = static_cast<int>(std::ceil(
      std::log2(world_size / min_cell_size)));
  return std::max(1, std::min(levels, 8));
}

void OctreeVisibilitySystem::SplitNode(OctreeNode* node) const {
  const core::Vec3f center = node->bounds.GetCenter();
  const core::Vec3f bmin   = node->bounds.GetMin();
  const core::Vec3f bmax   = node->bounds.GetMax();

  for (int i = 0; i < 8; ++i) {
    const core::Vec3f cmin(
        (i & 1) ? center.x : bmin.x,
        (i & 2) ? center.y : bmin.y,
        (i & 4) ? center.z : bmin.z);
    const core::Vec3f cmax(
        (i & 1) ? bmax.x : center.x,
        (i & 2) ? bmax.y : center.y,
        (i & 4) ? bmax.z : center.z);
    node->children[i]         = std::make_unique<OctreeNode>();
    node->children[i]->bounds = core::BBox3(cmin, cmax);
  }
}

void OctreeVisibilitySystem::Insert(OctreeNode* node, Renderable* r,
                                    int depth) {
  if (depth < max_depth_) {
    if (node->IsLeaf()) SplitNode(node);

    const core::BBox3& rbbox = r->GetWorldBBox();
    auto it = std::find_if(std::begin(node->children), std::end(node->children),
                           [&rbbox](const std::unique_ptr<OctreeNode>& c) {
                             return c->bounds.IsCompletelyIn(rbbox);
                           });
    if (it != std::end(node->children)) {
      Insert(it->get(), r, depth + 1);
      return;
    }
  }

  r->SetVisibilityId(reinterpret_cast<uintptr_t>(node));
  node->renderables.push_back(r);
}

void OctreeVisibilitySystem::Remove(OctreeNode* node, Renderable* r) {
  auto it = std::find(node->renderables.begin(), node->renderables.end(), r);
  if (it != node->renderables.end()) {
    *it = node->renderables.back();
    node->renderables.pop_back();
  }
}

void OctreeVisibilitySystem::AddRenderable(Renderable* r) {
  Insert(&root_, r, 0);
  r->SetVisibilitySystem(this);
}

void OctreeVisibilitySystem::RemoveRenderable(Renderable* r) {
  auto* node = reinterpret_cast<OctreeNode*>(r->GetVisibilityId());
  Remove(node, r);
  r->SetVisibilitySystem(nullptr);
}

void OctreeVisibilitySystem::OnRenderableMoved(Renderable* r) {
  auto* node = reinterpret_cast<OctreeNode*>(r->GetVisibilityId());
  if (node->bounds.IsCompletelyIn(r->GetWorldBBox())) return;
  Remove(node, r);
  Insert(&root_, r, 0);
}

void OctreeVisibilitySystem::CullNode(const OctreeNode* node,
                                      const core::ViewFrustum& frustum) const {
  if (!frustum.ContainsBBox(node->bounds)) return;

  for (Renderable* r : node->renderables) r->Enqueue();

  for (const auto& child : node->children) {
    if (child) CullNode(child.get(), frustum);
  }
}

void OctreeVisibilitySystem::CullAndEnqueue(
    const core::ViewFrustum& frustum) {
  CullNode(&root_, frustum);
}

void OctreeVisibilitySystem::ClearNode(OctreeNode* node) {
  for (Renderable* r : node->renderables) r->SetVisibilitySystem(nullptr);
  node->renderables.clear();

  for (auto& child : node->children) {
    if (child) {
      ClearNode(child.get());
      child.reset();
    }
  }
}

void OctreeVisibilitySystem::Clear() {
  ClearNode(&root_);
}

}  // namespace renderer
