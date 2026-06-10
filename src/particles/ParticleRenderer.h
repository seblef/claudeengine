#pragma once

#include <memory>
#include <vector>

#include "abstract/IndexBuffer.h"
#include "abstract/Shader.h"
#include "abstract/VideoDevice.h"
#include "core/Camera.h"
#include "particles/ParticleEmitter.h"

namespace particles {

// Renders solid (kGBuffer) ParticleEmitter instances into the G-buffer during
// the geometry pass so they receive full deferred lighting.
//
// Not a singleton. Created and owned by GameSystem / EditorSystem, then wired
// into the Renderer via Renderer::SetParticleRenderer().
//
// Typical usage:
//   // At scene init:
//   particle_renderer_->Register(emitter);
//   renderer.SetParticleRenderer(particle_renderer_.get());
//
//   // Each frame (inside Renderer::Update, end of geometry pass):
//   particle_renderer_->RenderGeometryPass(camera, renderable_infos_cb);
class ParticleRenderer {
 public:
  // Loads the particle G-buffer shader and builds the shared index buffer.
  // video must outlive this ParticleRenderer.
  explicit ParticleRenderer(abstract::VideoDevice* video);
  ~ParticleRenderer();

  ParticleRenderer(const ParticleRenderer&)            = delete;
  ParticleRenderer& operator=(const ParticleRenderer&) = delete;
  ParticleRenderer(ParticleRenderer&&)                 = delete;
  ParticleRenderer& operator=(ParticleRenderer&&)      = delete;

  // Adds an emitter to the render list.
  // Called by GameParticleSystem::OnAddedToScene.
  void Register(ParticleEmitter* emitter);

  // Removes an emitter from the render list.
  // Called by GameParticleSystem::OnRemovedFromScene.
  void Unregister(ParticleEmitter* emitter);

  // Renders all registered emitters whose blend_mode == kGBuffer.
  // Must be called while the G-buffer FBO is bound for writing.
  // renderable_infos_cb is filled with an identity world matrix per emitter.
  void RenderGeometryPass(const core::Camera& camera,
                          abstract::ConstantBuffer* renderable_infos_cb);

 private:
  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice*                 video_;
  // cppcheck-suppress unusedStructMember
  std::vector<ParticleEmitter*>          emitters_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<abstract::IndexBuffer> shared_ibo_;
  // cppcheck-suppress unusedStructMember
  abstract::Shader*                      shader_ = nullptr;
};

}  // namespace particles
