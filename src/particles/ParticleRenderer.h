#pragma once

#include <memory>
#include <vector>

#include "abstract/IndexBuffer.h"
#include "abstract/Shader.h"
#include "abstract/VideoDevice.h"
#include "core/Camera.h"
#include "particles/ParticleEmitter.h"

namespace particles {

// Renders ParticleEmitter instances.
//
// - kGBuffer emitters: drawn in the geometry pass via RenderGeometryPass().
// - kAdditive / kAlphaBlend emitters: drawn in the forward pass via
//   RenderForwardPass(). Alpha-blend emitters are sorted back-to-front by
//   distance from the camera before each draw.
//
// Not a singleton. Created and owned by GameSystem / EditorSystem, then wired
// into the Renderer via Renderer::SetParticleRenderer().
//
// Per-frame usage (via Renderer::Update):
//   particle_renderer_->BeginFrame();           // clears per-frame queue
//   // visibility pass calls EnqueueEmitter() per visible emitter
//   particle_renderer_->RenderGeometryPass(...);
//   particle_renderer_->RenderForwardPass(...);
class ParticleRenderer {
 public:
  // Loads particle shaders and builds the shared index buffer.
  // video must outlive this ParticleRenderer.
  explicit ParticleRenderer(abstract::VideoDevice* video);
  ~ParticleRenderer();

  ParticleRenderer(const ParticleRenderer&)            = delete;
  ParticleRenderer& operator=(const ParticleRenderer&) = delete;
  ParticleRenderer(ParticleRenderer&&)                 = delete;
  ParticleRenderer& operator=(ParticleRenderer&&)      = delete;

  // Clears the per-frame emitter queue. Must be called once at the start of
  // each frame, before the visibility pass enqueues renderables.
  void BeginFrame();

  // Adds an emitter to the per-frame render queue.
  // Called by renderer::ParticleRenderable::Enqueue() during visibility traversal.
  void EnqueueEmitter(ParticleEmitter* emitter);

  // Renders all queued emitters whose blend_mode == kGBuffer.
  // Must be called while the G-buffer FBO is bound for writing.
  // renderable_infos_cb is filled with an identity world matrix per emitter.
  void RenderGeometryPass(const core::Camera& camera,
                          abstract::ConstantBuffer* renderable_infos_cb);

  // Renders kAdditive and kAlphaBlend emitters into the currently bound HDR RT.
  // kAlphaBlend emitters are sorted back-to-front by emitter world position
  // distance from camera_pos before drawing.
  // Must be called while the emissive FBO is bound for writing.
  void RenderForwardPass(const core::Camera& camera,
                         abstract::ConstantBuffer* renderable_infos_cb);

 private:
  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice*                 video_;
  // cppcheck-suppress unusedStructMember
  std::vector<ParticleEmitter*>          frame_emitters_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<abstract::IndexBuffer> shared_ibo_;
  // cppcheck-suppress unusedStructMember
  abstract::Shader*                      gbuffer_shader_  = nullptr;
  // cppcheck-suppress unusedStructMember
  abstract::Shader*                      forward_shader_  = nullptr;
};

}  // namespace particles
