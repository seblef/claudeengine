#pragma once

#include <memory>
#include <vector>

#include "abstract/VideoDevice.h"
#include "game/GameObject.h"
#include "particles/ParticleEmitter.h"
#include "renderer/Light.h"
#include "renderer/ParticleRenderable.h"

namespace particles {
class ParticleSystemTemplate;
}

namespace game {

// Game object wrapping a ParticleSystemTemplate.
//
// Owns one ParticleEmitter and one ParticleRenderable per sub-system, and one
// renderer::Light per EmbeddedLightDesc declared in the template.
// Renderables are registered with the Renderer (visibility/culling) on scene
// entry; lights are registered with Renderer as well.  Light intensities
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

  // Registers renderables and lights with the Renderer (visibility system).
  void OnAddedToScene() override;

  // Unregisters renderables and lights from the Renderer.
  void OnRemovedFromScene() override;

  // Forwards the world transform to each emitter and light (with offset).
  void OnWorldTransformUpdated() override;

  [[nodiscard]] particles::ParticleSystemTemplate* GetTemplate() const;

  // Rebuilds all emitters and lights from the current template data.
  // If the object is already in the scene (renderables registered) it
  // unregisters, rebuilds, and re-registers transparently. Call this after
  // ParticleSystemTemplate::Reload() to propagate changes to a live instance.
  void ReloadFromTemplate();

 private:
  // cppcheck-suppress unusedStructMember
  particles::ParticleSystemTemplate*            template_;
  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice*                        video_;
  // cppcheck-suppress unusedStructMember
  std::vector<std::unique_ptr<particles::ParticleEmitter>>  emitters_;
  // cppcheck-suppress unusedStructMember
  std::vector<std::unique_ptr<renderer::ParticleRenderable>> renderables_;
  // cppcheck-suppress unusedStructMember
  std::vector<std::unique_ptr<renderer::Light>> lights_;
};

}  // namespace game
