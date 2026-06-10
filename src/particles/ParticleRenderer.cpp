#include "particles/ParticleRenderer.h"

#include <algorithm>
#include <vector>

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
  shader_ = video_->CreateShader("geometry/particle_gbuffer");

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
  if (shader_) shader_->Release();
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

  shader_->Activate();
  shared_ibo_->Bind();
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
    shader_->SetUniform2f("u_uv_size", uv_w, uv_h);

    // Bind particle texture at slot 0.
    abstract::Texture* tex = video_->CreateTexture(desc.texture);
    tex->Bind(0);

    emitter->GetVBO()->Bind();
    video_->RenderIndexed(particle_count * 6);

    tex->Release();
  }

  // Restore default depth function for subsequent passes.
  video_->SetDepthFunc(abstract::CompareFunc::kLess);
}

}  // namespace particles
