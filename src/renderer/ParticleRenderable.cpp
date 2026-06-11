#include "renderer/ParticleRenderable.h"

#include <algorithm>
#include <cmath>

#include "particles/ParticleRenderer.h"
#include "particles/ParticleSubSystemDesc.h"
#include "renderer/Renderer.h"

namespace renderer {

namespace {

// Returns a conservative local AABB for a single particle sub-system.
//
// Accounts for:
//   - spawn radius (emitter_radius)
//   - maximum travel distance (speed_max * lifetime_max)
//   - gravity-induced downward displacement (0.5 * gravity * lifetime_max²)
//   - maximum visual half-size (max(size_start_max, size_end_max) / 2)
//
// The Y-min is extended by the full gravity fall so downward-moving particles
// are not culled prematurely; all other axes use the symmetric horizontal reach.
core::BBox3 ComputeLocalBBox(const particles::ParticleSubSystemDesc& desc) {
  const float half_max_size =
      0.5f * std::max(desc.size_start_max, desc.size_end_max);
  const float travel = desc.speed_max * desc.lifetime_max;
  const float fall   = 0.5f * desc.gravity * desc.lifetime_max * desc.lifetime_max;
  const float r      = desc.emitter_radius + travel + half_max_size;
  return core::BBox3(-r, -(r + fall), -r, r, r, r);
}

}  // namespace

ParticleRenderable::ParticleRenderable(particles::ParticleEmitter* emitter,
                                       const core::Mat4f& world_matrix,
                                       bool always_visible)
    : Renderable(ComputeLocalBBox(emitter->GetDesc()), world_matrix, always_visible),
      emitter_(emitter) {}

void ParticleRenderable::Enqueue() {
  particles::ParticleRenderer* pr =
      Renderer::Instance().GetParticleRenderer();
  if (pr) pr->EnqueueEmitter(emitter_);
}

}  // namespace renderer
