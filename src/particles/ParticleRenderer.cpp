#include "particles/ParticleRenderer.h"

#include <algorithm>
#include <vector>

#include "abstract/BlendFactor.h"
#include "abstract/BufferUsage.h"
#include "abstract/CompareFunc.h"
#include "abstract/IndexType.h"
#include "abstract/PrimitiveType.h"
#include "abstract/Texture.h"
#include "particles/ParticleBlendMode.h"

namespace particles {

namespace {

// Maximum number of particles the shared IBO supports per emitter draw call.
// 65536 particles × 4 vertices = 262144 vertices, fits within uint32 range.
constexpr int kMaxIBOParticles = 65536;

}  // namespace

ParticleRenderer::ParticleRenderer(abstract::VideoDevice* video) : video_(video) {
  gbuffer_shader_ = video_->CreateShader("geometry/particle_gbuffer");
  forward_shader_ = video_->CreateShader("forward/particle_forward");

  // Build the shared index buffer: pattern 0,1,2, 0,2,3 per quad.
  const int num_indices = kMaxIBOParticles * 6;
  std::vector<uint32_t> indices(num_indices);
  for (int i = 0; i < kMaxIBOParticles; ++i) {
    const uint32_t base_v = static_cast<uint32_t>(i * 4);
    const int      base_i = i * 6;
    indices[base_i + 0] = base_v + 0;
    indices[base_i + 1] = base_v + 1;
    indices[base_i + 2] = base_v + 2;
    indices[base_i + 3] = base_v + 0;
    indices[base_i + 4] = base_v + 2;
    indices[base_i + 5] = base_v + 3;
  }
  shared_ibo_ = video_->CreateIndexBuffer(
      abstract::IndexType::kUInt32, num_indices,
      abstract::BufferUsage::kImmutable,
      indices.data());
}

ParticleRenderer::~ParticleRenderer() {
  if (gbuffer_shader_) gbuffer_shader_->Release();
  if (forward_shader_) forward_shader_->Release();
}

void ParticleRenderer::Register(ParticleEmitter* emitter) {
  emitters_.push_back(emitter);
}

void ParticleRenderer::Unregister(ParticleEmitter* emitter) {
  emitters_.erase(std::remove(emitters_.begin(), emitters_.end(), emitter),
                  emitters_.end());
}

void ParticleRenderer::RenderGeometryPass(
    const core::Camera& /*camera*/,
    abstract::ConstantBuffer* /*renderable_infos_cb*/) {
  if (emitters_.empty()) return;

  // Depth write ON, LEQUAL test — billboards share depth with their own quads.
  video_->SetDepthWriteEnabled(true);
  video_->SetDepthTestEnabled(true);
  video_->SetDepthFunc(abstract::CompareFunc::kLessEqual);

  gbuffer_shader_->Activate();
  video_->SetPrimitiveType(abstract::PrimitiveType::kTriangleList);
  video_->SetIndexType(abstract::IndexType::kUInt32);

  for (const ParticleEmitter* emitter : emitters_) {
    if (emitter->GetDesc().blend_mode != ParticleBlendMode::kGBuffer) continue;
    const int particle_count = emitter->GetParticleCount();
    if (particle_count <= 0) continue;

    const ParticleSubSystemDesc& desc = emitter->GetDesc();

    // Upload sprite-sheet cell size.
    const float uv_w = 1.f / static_cast<float>(std::max(1, desc.sprite_cols));
    const float uv_h = 1.f / static_cast<float>(std::max(1, desc.sprite_rows));
    gbuffer_shader_->SetUniform2f("u_uv_size", uv_w, uv_h);

    abstract::Texture* tex = emitter->GetTexture();
    if (!tex) continue;
    tex->Bind(0);

    emitter->GetVBO()->Bind();
    shared_ibo_->Bind();
    video_->RenderIndexed(particle_count * 6);
  }

  // Restore default depth function for subsequent passes.
  video_->SetDepthFunc(abstract::CompareFunc::kLess);
}

void ParticleRenderer::RenderForwardPass(
    const core::Camera& camera,
    abstract::ConstantBuffer* /*renderable_infos_cb*/) {
  if (emitters_.empty()) return;

  // Collect additive and alpha-blend emitters.
  std::vector<ParticleEmitter*> additive_emitters;
  std::vector<ParticleEmitter*> alpha_emitters;
  for (ParticleEmitter* emitter : emitters_) {
    const ParticleBlendMode mode = emitter->GetDesc().blend_mode;
    if (mode == ParticleBlendMode::kAdditive)
      additive_emitters.push_back(emitter);
    else if (mode == ParticleBlendMode::kAlphaBlend)
      alpha_emitters.push_back(emitter);
  }

  if (additive_emitters.empty() && alpha_emitters.empty()) return;

  // Sort kAlphaBlend emitters back-to-front by emitter world-position distance.
  const core::Vec3f cam_pos = camera.GetPosition();
  std::sort(alpha_emitters.begin(), alpha_emitters.end(),
            [&cam_pos](const ParticleEmitter* a, const ParticleEmitter* b) {
              const core::Vec3f da = a->GetWorldPosition() - cam_pos;
              const core::Vec3f db = b->GetWorldPosition() - cam_pos;
              return (da.x * da.x + da.y * da.y + da.z * da.z) >
                     (db.x * db.x + db.y * db.y + db.z * db.z);
            });

  // Depth test LEQUAL, depth write OFF — transparent particles read depth only.
  video_->SetDepthTestEnabled(true);
  video_->SetDepthWriteEnabled(false);
  video_->SetDepthFunc(abstract::CompareFunc::kLessEqual);

  forward_shader_->Activate();
  video_->SetPrimitiveType(abstract::PrimitiveType::kTriangleList);
  video_->SetIndexType(abstract::IndexType::kUInt32);

  // Draw kAdditive emitters: src=ONE, dst=ONE (emissive by nature, no lighting).
  if (!additive_emitters.empty()) {
    video_->SetBlendEnabled(true, abstract::BlendFactor::kOne,
                            abstract::BlendFactor::kOne);
    forward_shader_->SetUniformInt("u_lit", 0);

    for (const ParticleEmitter* emitter : additive_emitters) {
      const int particle_count = emitter->GetParticleCount();
      if (particle_count <= 0) continue;

      const ParticleSubSystemDesc& desc = emitter->GetDesc();
      const float uv_w = 1.f / static_cast<float>(std::max(1, desc.sprite_cols));
      const float uv_h = 1.f / static_cast<float>(std::max(1, desc.sprite_rows));
      forward_shader_->SetUniform2f("u_uv_size", uv_w, uv_h);

      abstract::Texture* tex = emitter->GetTexture();
      if (!tex) continue;
      tex->Bind(0);
      emitter->GetVBO()->Bind();
      shared_ibo_->Bind();
      video_->RenderIndexed(particle_count * 6);
    }
  }

  // Draw kAlphaBlend emitters: src=SRC_ALPHA, dst=ONE_MINUS_SRC_ALPHA.
  if (!alpha_emitters.empty()) {
    video_->SetBlendEnabled(true, abstract::BlendFactor::kSrcAlpha,
                            abstract::BlendFactor::kOneMinusSrcAlpha);

    for (const ParticleEmitter* emitter : alpha_emitters) {
      const int particle_count = emitter->GetParticleCount();
      if (particle_count <= 0) continue;

      const ParticleSubSystemDesc& desc = emitter->GetDesc();
      const float uv_w = 1.f / static_cast<float>(std::max(1, desc.sprite_cols));
      const float uv_h = 1.f / static_cast<float>(std::max(1, desc.sprite_rows));
      forward_shader_->SetUniform2f("u_uv_size", uv_w, uv_h);
      forward_shader_->SetUniformInt("u_lit", desc.lit ? 1 : 0);

      abstract::Texture* tex = emitter->GetTexture();
      if (!tex) continue;
      tex->Bind(0);
      emitter->GetVBO()->Bind();
      shared_ibo_->Bind();
      video_->RenderIndexed(particle_count * 6);
    }
  }

  // Restore render state.
  video_->SetBlendEnabled(false);
  video_->SetDepthFunc(abstract::CompareFunc::kLess);
  video_->SetDepthWriteEnabled(true);
}

}  // namespace particles
