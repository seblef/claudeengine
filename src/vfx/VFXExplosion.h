#pragma once

#include <memory>
#include <vector>

#include "core/Vec3f.h"
#include "particles/ParticleSubSystemDesc.h"
#include "vfx/IVFXEffect.h"
#include "vfx/VFXExplosionDesc.h"

namespace game {
class GameLight;
}  // namespace game

namespace particles {
class ParticleEmitter;
class ParticleSystemTemplate;
}  // namespace particles

namespace renderer {
class ParticleRenderable;
}  // namespace renderer

namespace vfx {

// Composited, self-expiring explosion: a fireball/smoke particle burst, a
// short-lived high-intensity point-light flash, a one-shot outward physics
// impulse to nearby dynamic bodies, and a camera shake that falls off with
// distance. This is the effect that proves VFXSystem's composition model
// end-to-end (particles + renderer light + physics query + camera shake in
// one instance) and the payoff of the wreck state.
//
// The shockwave impulse and the screen shake are one-shot events fired from
// Play(); Update(dt) only advances the particle sub-systems and the
// transient light's countdown. The whole effect self-expires once
// desc.particle_duration has elapsed, releasing the light (if it hasn't
// already expired on its own, shorter, desc.light_lifetime).
class VFXExplosion : public IVFXEffect {
 public:
  // Asset stem of the particle template supplying the fireball + smoke
  // sub-systems (see the note on explosion_template below).
  // cppcheck-suppress unusedStructMember ; used by future explosion triggers
  static constexpr const char* kExplosionTemplateName = "explosion";

  // explosion_template is non-owning and must outlive this VFXExplosion; the
  // caller is responsible for loading it once (e.g. via
  // particles::ParticleSystemTemplate::GetOrLoad(kExplosionTemplateName, ...))
  // and holding it for as long as explosions might be triggered, for the same
  // reason VFXScrape never loads its spark template itself — see
  // VFXScrape.h's constructor doc comment. May be nullptr (e.g. no Renderer
  // instanced), in which case no particles are emitted but the light,
  // shockwave, and shake still fire normally.
  VFXExplosion(const VFXExplosionDesc& desc,
              particles::ParticleSystemTemplate* explosion_template);

  ~VFXExplosion() override;

  // Spawns the fireball/smoke particle burst, applies the physics shockwave
  // impulse, triggers the screen shake, and starts the light flash — all at
  // world_pos. direction is unused (explosions are radial).
  void Play(const core::Vec3f& world_pos, const core::Vec3f& direction) override;

  // Ticks the particle sub-systems and the light countdown.
  void Update(float dt) override;

  [[nodiscard]] bool IsFinished() const override { return finished_; }

 private:
  void ApplyShockwave(const core::Vec3f& world_pos);
  void TriggerShake(const core::Vec3f& world_pos);
  void SpawnLight(const core::Vec3f& world_pos);

  // cppcheck-suppress unusedStructMember
  VFXExplosionDesc desc_;
  // Non-owning; see the constructor's doc comment.
  // cppcheck-suppress unusedStructMember
  // cppcheck-suppress uninitMemberVarPrivate ; initialized in VFXExplosion.cpp,
  // not visible to cppcheck when this header is checked from a TU that only
  // declares (not defines) the constructor, e.g. the unit test file.
  particles::ParticleSystemTemplate* explosion_template_;

  // Owned by value (one copy per sub-system, taken from explosion_template_)
  // so each ParticleEmitter's stored desc reference stays valid; populated
  // once in Play() and never resized afterwards.
  // cppcheck-suppress unusedStructMember
  std::vector<particles::ParticleSubSystemDesc>              sub_descs_;
  // cppcheck-suppress unusedStructMember
  std::vector<std::unique_ptr<particles::ParticleEmitter>>    emitters_;
  // cppcheck-suppress unusedStructMember
  std::vector<std::unique_ptr<renderer::ParticleRenderable>>  renderables_;

  // Transient point-light flash; released once light_elapsed_ reaches
  // desc_.light_lifetime, or early in the destructor if the effect is torn
  // down before that.
  std::unique_ptr<game::GameLight> light_;
  // cppcheck-suppress unusedStructMember
  float light_elapsed_ = 0.f;

  // cppcheck-suppress unusedStructMember
  float elapsed_  = 0.f;
  // cppcheck-suppress unusedStructMember
  bool  finished_ = true;
};

}  // namespace vfx
