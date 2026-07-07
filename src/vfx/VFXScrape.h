#pragma once

#include <memory>

#include "core/Vec3f.h"
#include "particles/ParticleSubSystemDesc.h"
#include "physics/VehicleDesc.h"
#include "vfx/IVFXEffect.h"

namespace audio {
class ResourceManager;
class Sound;
class SoundManager;
class VirtualSoundInstance;
}  // namespace audio

namespace particles {
class ParticleEmitter;
class ParticleSystemTemplate;
}  // namespace particles

namespace renderer {
class ParticleRenderable;
}  // namespace renderer

namespace vfx {

// Continuous scrape effect: directional spark particles + a looping metal
// screech, driven every physics step by live contact data.
//
// Unlike a typical IVFXEffect (fire once via Play(), decay via Update(dt)),
// a scrape is stateful: it must be fed live contact point / surface normal /
// relative velocity for as long as the panel keeps sliding against geometry.
// Play()/Update()/IsFinished() satisfy the IVFXEffect contract so VFXSystem
// can still own and reap the instance normally, but the owner (typically
// game::VehicleScrapeEffect) drives the actual visuals/audio by calling
// UpdateContact() once per physics step, and calls Stop() the moment contact
// ends or the sliding speed drops below desc.min_speed — Update(dt) itself
// only ticks the particle simulation and never decides to end the effect.
class VFXScrape : public IVFXEffect {
 public:
  // Asset stem of the particle template loaded by whoever constructs the
  // long-lived spark_template passed to the constructor below (typically
  // game::VehicleScrapeEffect, once per vehicle) — see the note on
  // spark_template for why VFXScrape itself never loads this.
  // cppcheck-suppress unusedStructMember ; used by vfx::VehicleScrapeEffect
  static constexpr const char* kSparkTemplateName = "sparks";

  // spark_template is non-owning and must outlive this VFXScrape; it supplies
  // the authored spark visuals (lifetime, size, color gradient — direction and
  // emission_rate are overwritten every UpdateContact() call). May be nullptr
  // (e.g. no Renderer instanced), in which case no particles are emitted.
  //
  // Deliberately NOT loaded internally via ParticleSystemTemplate::GetOrLoad():
  // this class is constructed fresh every time a scrape starts and destroyed
  // when it ends, so loading (and releasing) the template here would reload
  // it from disk on every single scrape start — core::Resource::Release()
  // evicts and deletes the moment its ref count hits zero. The caller instead
  // holds one persistent reference for as long as scraping might occur (e.g.
  // for the vehicle's whole lifetime), so the template is parsed once and
  // shared across every scrape session.
  VFXScrape(const physics::ScrapeDesc& desc,
           audio::SoundManager* sound_manager,
           audio::ResourceManager* resource_manager,
           particles::ParticleSystemTemplate* spark_template);

  ~VFXScrape() override;

  // Starts the looping screech (if sound managers are available) at world_pos,
  // initially oriented along direction. Spark visuals come from spark_template
  // (see the constructor); no disk/GPU load happens here.
  void Play(const core::Vec3f& world_pos, const core::Vec3f& direction) override;

  // Ticks the spark emitter's simulation. Does not decide when the effect
  // ends; see Stop().
  void Update(float dt) override;

  [[nodiscard]] bool IsFinished() const override { return finished_; }

  // Repositions the spark emitter at world_point, orients spark emission
  // along the reflection of relative_velocity off surface_normal, and scales
  // spark rate / screech gain by relative_velocity's magnitude, normalised
  // between desc.min_speed and desc.max_speed. Call once per physics step
  // while contact persists.
  void UpdateContact(const core::Vec3f& world_point,
                     const core::Vec3f& surface_normal,
                     const core::Vec3f& relative_velocity);

  // Stops the looping screech and spark emission immediately and marks the
  // effect finished so VFXSystem reaps it on the next Update(). Safe to call
  // more than once.
  void Stop();

  // Normalises speed into [0, 1] between desc.min_speed and desc.max_speed.
  // Exposed for unit testing.
  [[nodiscard]] static float ComputeIntensity(float speed,
                                              const physics::ScrapeDesc& desc);

  // Reflects incoming (need not be normalised) off normal (must be
  // normalised), returning a normalised direction. Exposed for unit testing.
  [[nodiscard]] static core::Vec3f ReflectDirection(const core::Vec3f& incoming,
                                                    const core::Vec3f& normal);

 private:
  // cppcheck-suppress unusedStructMember
  physics::ScrapeDesc     desc_;
  // cppcheck-suppress unusedStructMember
  // cppcheck-suppress uninitMemberVarPrivate ; initialized in VFXScrape.cpp,
  // not visible to cppcheck when this header is checked from a TU that only
  // declares (not defines) the constructor, e.g. the unit test file.
  audio::SoundManager*    sound_manager_;
  // cppcheck-suppress unusedStructMember
  // cppcheck-suppress uninitMemberVarPrivate
  audio::ResourceManager* resource_manager_;
  // Non-owning; see the constructor's doc comment.
  // cppcheck-suppress unusedStructMember
  // cppcheck-suppress uninitMemberVarPrivate
  particles::ParticleSystemTemplate* spark_template_;

  // cppcheck-suppress unusedStructMember
  audio::Sound* sound_ = nullptr;
  // Non-owning; valid while the loop is playing.
  // cppcheck-suppress unusedStructMember
  audio::VirtualSoundInstance* instance_ = nullptr;

  // Owned by value (not a template reference) so direction/emission_rate can
  // be mutated every UpdateContact() call; declared before spark_emitter_
  // since ParticleEmitter stores a reference to it.
  // cppcheck-suppress unusedStructMember
  particles::ParticleSubSystemDesc spark_desc_;
  std::unique_ptr<particles::ParticleEmitter>   spark_emitter_;
  std::unique_ptr<renderer::ParticleRenderable> spark_renderable_;

  // cppcheck-suppress unusedStructMember
  bool finished_ = true;
};

}  // namespace vfx
