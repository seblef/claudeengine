#pragma once

#include "particles/ParticleEmitter.h"
#include "renderer/Renderable.h"

namespace renderer {

// A Renderable wrapping one particle sub-system emitter.
//
// Participates in the visibility system: the emitter is only scheduled for
// rendering when its conservative world-space AABB intersects the view frustum.
//
// Enqueue() registers the emitter with the active ParticleRenderer's per-frame
// queue so that RenderGeometryPass / RenderForwardPass can draw it.
//
// The local bbox is computed conservatively from the emitter descriptor at
// construction time (emitter spawn radius + max travel + gravity fall + half
// max visual size); see ComputeLocalBBox in the implementation for details.
class ParticleRenderable : public Renderable {
 public:
  // Computes a conservative local AABB from emitter->GetDesc().
  // emitter must outlive this object.
  ParticleRenderable(particles::ParticleEmitter* emitter,
                     const core::Mat4f& world_matrix,
                     bool always_visible);

  // Registers the emitter with the active ParticleRenderer's per-frame queue.
  void Enqueue() override;

  // Stores an opaque key used to identify this renderable for selection
  // highlight. Callers pass their own address; WireframeRenderer::IsHighlighted()
  // compares pointers without dereferencing.
  void SetGizmoKey(const void* key) { gizmo_key_ = key; }

 private:
  // cppcheck-suppress unusedStructMember
  particles::ParticleEmitter* emitter_;
  // cppcheck-suppress unusedStructMember
  const void* gizmo_key_ = nullptr;
};

}  // namespace renderer
