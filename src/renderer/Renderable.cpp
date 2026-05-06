#include "renderer/Renderable.h"

namespace renderer {

Renderable::Renderable(const core::BBox3& local_bbox,
                       const core::Mat4f& world_matrix,
                       bool always_visible)
    : local_bbox_(local_bbox),
      world_bbox_(local_bbox * world_matrix),
      world_matrix_(world_matrix),
      always_visible_(always_visible) {}

Renderable::Renderable(const Renderable* other)
    : local_bbox_(other->local_bbox_),
      world_bbox_(other->world_bbox_),
      world_matrix_(other->world_matrix_),
      always_visible_(other->always_visible_) {}

const core::Mat4f& Renderable::GetWorldMatrix() const { return world_matrix_; }

void Renderable::SetWorldMatrix(const core::Mat4f& m) {
  world_matrix_ = m;
  world_bbox_   = local_bbox_ * world_matrix_;
}

const core::BBox3& Renderable::GetLocalBBox() const { return local_bbox_; }
const core::BBox3& Renderable::GetWorldBBox() const { return world_bbox_; }

uint64_t Renderable::GetLastFrame() const       { return last_frame_; }
void          Renderable::SetLastFrame(uint64_t f) { last_frame_ = f; }

bool Renderable::IsAlwaysVisible() const { return always_visible_; }

}  // namespace renderer
