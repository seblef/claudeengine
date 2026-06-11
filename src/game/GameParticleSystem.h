#pragma once

#include <memory>
#include <vector>

#include "abstract/VideoDevice.h"
#include "game/GameObject.h"
#include "particles/ParticleEmitter.h"
#include "renderer/Light.h"

namespace particles {
class ParticleSystemTemplate;
}

namespace game {

// Game object wrapping a ParticleSystemTemplate.
//
// Owns one ParticleEmitter per sub-system and one renderer::Light per
// EmbeddedLightDesc declared in the template. Emitters are registered with
// ParticleRenderer on scene entry; lights with Renderer. Light intensities
// flicker each frame using the per-descriptor frequency and phase parameters.
class GameParticleSystem : public GameObject {
 public:
  // tmpl must not be null. Calls tmpl->AddRef().
  // video is used to create the ParticleEmitter VBOs and is stored for Copy().
  GameParticleSystem(particles::ParticleSystemTemplate* tmpl,
                     abstract::VideoDevice* video);

  // Calls tmpl->Release().
  ~GameParticleSystem() override;

  GameParticleSystem(const GameParticleSystem&)            = delete;
  GameParticleSystem& operator=(const GameParticleSystem&) = delete;
  GameParticleSystem(GameParticleSystem&&)                 = delete;
  GameParticleSystem& operator=(GameParticleSystem&&)      = delete;

  void Accept(GameObjectVisitor& visitor) override { visitor.Visit(*this); }

  // Returns a new GameParticleSystem from the same template, placed at position.
  [[nodiscard]] std::unique_ptr<GameObject> Copy(
      const core::Vec3f& position) const override;

  // Ticks all emitters and updates embedded light intensities.
  // Must be called once per frame from GameSystem::Update().
  void Update(float time, float dt);

  // Registers emitters with ParticleRenderer and lights with Renderer.
  void OnAddedToScene() override;

  // Unregisters emitters and lights.
  void OnRemovedFromScene() override;

  // Forwards the world transform to each emitter and light (with offset).
  void OnWorldTransformUpdated() override;

  [[nodiscard]] particles::ParticleSystemTemplate* GetTemplate() const;

 private:
  // cppcheck-suppress unusedStructMember
  particles::ParticleSystemTemplate*            template_;
  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice*                        video_;
  // cppcheck-suppress unusedStructMember
  std::vector<std::unique_ptr<particles::ParticleEmitter>> emitters_;
  // cppcheck-suppress unusedStructMember
  std::vector<std::unique_ptr<renderer::Light>> lights_;
};

}  // namespace game
