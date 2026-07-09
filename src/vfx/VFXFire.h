#pragma once

#include <memory>
#include <vector>

#include "core/Mat4f.h"
#include "core/Vec3f.h"
#include "particles/ParticleSubSystemDesc.h"
#include "vfx/IVFXEffect.h"

namespace particles {
class ParticleEmitter;
class ParticleSystemTemplate;
}  // namespace particles

namespace renderer {
class ParticleRenderable;
}  // namespace renderer

namespace vfx {

// Continuous fire + smoke effect attached to a vehicle body once damage is
// high: unlike a typical IVFXEffect (fire once via Play(), decay via
// Update(dt)), this effect loops indefinitely and must be repositioned every
// frame by its owner (typically vfx::VehicleFireEffect) via
// SetWorldTransform(), for as long as the vehicle stays above its damage
// threshold. Play()/Update()/IsFinished() satisfy the IVFXEffect contract so
// VFXSystem can still own and reap the instance normally, but Update(dt)
// itself only ticks the particle sub-systems and never decides to end the
// effect — call Stop() when the fire should be extinguished.
class VFXFire : public IVFXEffect {
 public:
  // Asset stem of the particle template loaded by whoever constructs the
  // long-lived fire_template passed to the constructor below (typically
  // vfx::VehicleFireEffect, once per vehicle) — see the note on fire_template
  // for why VFXFire itself never loads this.
  // cppcheck-suppress unusedStructMember ; used by vfx::VehicleFireEffect
  static constexpr const char* kFireTemplateName = "fire";

  // fire_template is non-owning and must outlive this VFXFire; it supplies
  // every sub-system authored for the effect (e.g. a flame layer plus a
  // smoke layer). May be nullptr (e.g. no Renderer instanced), in which case
  // no particles are emitted.
  //
  // heat_distortion is carried for authoring intent only: no screen-space
  // post-process pass exists yet in the engine to consume it (see
  // physics::FireDesc::heat_distortion). It is exposed via
  // IsHeatDistortionEnabled() so a future post-process pass can query it.
  VFXFire(particles::ParticleSystemTemplate* fire_template, bool heat_distortion);

  ~VFXFire() override;

  // Starts fire/smoke emission at world_pos. direction is unused — each
  // sub-system emits along its own authored direction, not a caller-supplied
  // one (unlike vfx::VFXScrape, which reorients sparks every contact).
  void Play(const core::Vec3f& world_pos, const core::Vec3f& direction) override;

  // Ticks every sub-system's particle simulation. Does not decide when the
  // effect ends; see Stop().
  void Update(float dt) override;

  [[nodiscard]] bool IsFinished() const override { return finished_; }

  // Repositions every sub-system's emitter/renderable at world_transform.
  // Call once per frame while the effect should keep following its target.
  void SetWorldTransform(const core::Mat4f& world_transform);

  // Stops all emission immediately and marks the effect finished so
  // VFXSystem reaps it on the next Update(). Safe to call more than once.
  void Stop();

  [[nodiscard]] bool IsHeatDistortionEnabled() const { return heat_distortion_; }

 private:
  // Non-owning; see the constructor's doc comment.
  // cppcheck-suppress unusedStructMember
  // cppcheck-suppress uninitMemberVarPrivate ; initialized in VFXFire.cpp,
  // not visible to cppcheck when this header is checked from a TU that only
  // declares (not defines) the constructor, e.g. the unit test file.
  particles::ParticleSystemTemplate* fire_template_;
  // cppcheck-suppress unusedStructMember
  // cppcheck-suppress uninitMemberVarPrivate
  bool heat_distortion_;

  // Owned by value (one copy per sub-system, taken from fire_template_) so
  // each ParticleEmitter's stored desc reference stays valid; populated once
  // in Play() and never resized afterwards.
  // cppcheck-suppress unusedStructMember
  std::vector<particles::ParticleSubSystemDesc>              sub_descs_;
  // cppcheck-suppress unusedStructMember
  std::vector<std::unique_ptr<particles::ParticleEmitter>>   emitters_;
  // cppcheck-suppress unusedStructMember
  std::vector<std::unique_ptr<renderer::ParticleRenderable>> renderables_;

  // cppcheck-suppress unusedStructMember
  bool finished_ = true;
};

}  // namespace vfx
