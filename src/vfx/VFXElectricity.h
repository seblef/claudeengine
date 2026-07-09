#pragma once

#include <memory>
#include <vector>

#include "core/Vec3f.h"
#include "particles/ParticleSubSystemDesc.h"
#include "vfx/IVFXEffect.h"
#include "vfx/VFXElectricityDesc.h"

namespace particles {
class ParticleEmitter;
class ParticleSystemTemplate;
}  // namespace particles

namespace renderer {
class ParticleRenderable;
}  // namespace renderer

namespace vfx {

// One-shot electric arc connecting two world-space points: world_pos (start)
// and world_pos + direction * desc.arc_length (end). Visualised as a chain of
// spark-particle bursts placed along the segment between the two points,
// giving a jagged, crackling look without a dedicated line-rendering pass.
//
// Asset stem of the particle template supplying the spark sub-system (see the
// note on spark_template below).
class VFXElectricity : public IVFXEffect {
 public:
  // cppcheck-suppress unusedStructMember ; used by editor::VFXPanel's preview
  static constexpr const char* kElectricityTemplateName = "electricity";

  // spark_template is non-owning and must outlive this VFXElectricity; the
  // caller is responsible for loading it once (e.g. via
  // particles::ParticleSystemTemplate::GetOrLoad(kElectricityTemplateName, ...))
  // and holding it for as long as arcs might be triggered — same reasoning as
  // VFXExplosion's explosion_template. May be nullptr (e.g. no Renderer
  // instanced), in which case no particles are emitted.
  VFXElectricity(const VFXElectricityDesc& desc,
                particles::ParticleSystemTemplate* spark_template);

  ~VFXElectricity() override;

  // Spawns arc_segments spark bursts evenly spaced between world_pos and
  // world_pos + direction * desc.arc_length.
  void Play(const core::Vec3f& world_pos, const core::Vec3f& direction) override;

  // Ticks every node emitter's particle simulation.
  void Update(float dt) override;

  [[nodiscard]] bool IsFinished() const override { return finished_; }

 private:
  // cppcheck-suppress unusedStructMember
  VFXElectricityDesc desc_;
  // Non-owning; see the constructor's doc comment.
  // cppcheck-suppress unusedStructMember
  // cppcheck-suppress uninitMemberVarPrivate ; initialized in VFXElectricity.cpp,
  // not visible to cppcheck when this header is checked from a TU that only
  // declares (not defines) the constructor, e.g. the unit test file.
  particles::ParticleSystemTemplate* spark_template_;

  // Owned by value (one copy per arc node) so each ParticleEmitter's stored
  // desc reference stays valid; populated once in Play() and never resized
  // afterwards.
  // cppcheck-suppress unusedStructMember
  std::vector<particles::ParticleSubSystemDesc>              node_descs_;
  // cppcheck-suppress unusedStructMember
  std::vector<std::unique_ptr<particles::ParticleEmitter>>    emitters_;
  // cppcheck-suppress unusedStructMember
  std::vector<std::unique_ptr<renderer::ParticleRenderable>>  renderables_;

  // cppcheck-suppress unusedStructMember
  float elapsed_  = 0.f;
  // cppcheck-suppress unusedStructMember
  bool  finished_ = true;
};

}  // namespace vfx
