#include "vfx/VFXElectricity.h"

#include <algorithm>
#include <utility>

#include "abstract/VideoDevice.h"
#include "core/Mat4f.h"
#include "particles/ParticleEmitter.h"
#include "particles/ParticleSystemTemplate.h"
#include "renderer/ParticleRenderable.h"
#include "renderer/Renderer.h"

namespace vfx {

VFXElectricity::VFXElectricity(const VFXElectricityDesc& desc,
                               particles::ParticleSystemTemplate* spark_template)
    : desc_(desc), spark_template_(spark_template) {}

VFXElectricity::~VFXElectricity() {
  if (renderer::Renderer::IsInstanced()) {
    for (auto& renderable : renderables_) {
      if (renderable) renderer::Renderer::Instance().RemoveRenderable(renderable.get());
    }
  }
}

void VFXElectricity::Play(const core::Vec3f& world_pos, const core::Vec3f& direction) {
  finished_ = false;
  elapsed_  = 0.f;

  if (!renderer::Renderer::IsInstanced() || !spark_template_ ||
      spark_template_->GetSubSystems().empty()) {
    return;
  }

  abstract::VideoDevice* video = renderer::Renderer::Instance().GetVideoDevice();
  const particles::ParticleSubSystemDesc& base = spark_template_->GetSubSystems()[0];

  const float dir_len = direction.Length();
  const core::Vec3f dir = dir_len > 1e-4f ? direction * (1.f / dir_len) : core::Vec3f::kAxisY;
  const core::Vec3f end = world_pos + dir * desc_.arc_length;

  const int segment_count = std::max(1, desc_.arc_segments);
  node_descs_.clear();
  node_descs_.reserve(segment_count);
  emitters_.reserve(segment_count);
  renderables_.reserve(segment_count);

  for (int i = 0; i < segment_count; ++i) {
    const float t = (segment_count == 1)
        ? 0.5f : static_cast<float>(i) / static_cast<float>(segment_count - 1);
    const core::Vec3f node_pos = world_pos + (end - world_pos) * t;

    particles::ParticleSubSystemDesc node_desc = base;
    node_desc.emission_rate = desc_.spark_rate / static_cast<float>(segment_count);
    node_descs_.push_back(std::move(node_desc));

    const core::Mat4f transform = core::Mat4f::Translation(node_pos);
    auto emitter = std::make_unique<particles::ParticleEmitter>(node_descs_.back(), video);
    emitter->SetWorldTransform(transform);
    auto renderable = std::make_unique<renderer::ParticleRenderable>(
        emitter.get(), transform, /*always_visible=*/false);
    renderer::Renderer::Instance().AddRenderable(renderable.get());

    emitters_.push_back(std::move(emitter));
    renderables_.push_back(std::move(renderable));
  }
}

void VFXElectricity::Update(float dt) {
  if (finished_) return;

  elapsed_ += dt;
  for (auto& emitter : emitters_) {
    emitter->Update(dt);
    emitter->UploadToGPU();
  }

  if (elapsed_ >= desc_.duration) finished_ = true;
}

}  // namespace vfx
