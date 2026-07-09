#include "vfx/VFXFire.h"

#include <utility>

#include "abstract/VideoDevice.h"
#include "particles/ParticleEmitter.h"
#include "particles/ParticleSystemTemplate.h"
#include "renderer/ParticleRenderable.h"
#include "renderer/Renderer.h"

namespace vfx {

VFXFire::VFXFire(particles::ParticleSystemTemplate* fire_template, bool heat_distortion)
    : fire_template_(fire_template), heat_distortion_(heat_distortion) {}

VFXFire::~VFXFire() {
  Stop();
}

void VFXFire::Play(const core::Vec3f& world_pos, const core::Vec3f& /*direction*/) {
  finished_ = false;

  if (renderer::Renderer::IsInstanced() && fire_template_) {
    abstract::VideoDevice* video = renderer::Renderer::Instance().GetVideoDevice();
    const core::Mat4f transform = core::Mat4f::Translation(world_pos);

    sub_descs_ = fire_template_->GetSubSystems();
    emitters_.reserve(sub_descs_.size());
    renderables_.reserve(sub_descs_.size());
    for (const auto& sub_desc : sub_descs_) {
      auto emitter = std::make_unique<particles::ParticleEmitter>(sub_desc, video);
      emitter->SetWorldTransform(transform);
      auto renderable = std::make_unique<renderer::ParticleRenderable>(
          emitter.get(), transform, /*always_visible=*/false);
      renderer::Renderer::Instance().AddRenderable(renderable.get());
      emitters_.push_back(std::move(emitter));
      renderables_.push_back(std::move(renderable));
    }
  }
}

void VFXFire::Update(float dt) {
  if (finished_) return;
  for (auto& emitter : emitters_) {
    emitter->Update(dt);
    emitter->UploadToGPU();
  }
}

void VFXFire::SetWorldTransform(const core::Mat4f& world_transform) {
  if (finished_) return;
  for (size_t i = 0; i < emitters_.size(); ++i) {
    emitters_[i]->SetWorldTransform(world_transform);
    renderables_[i]->SetWorldMatrix(world_transform);
  }
}

void VFXFire::Stop() {
  if (finished_) return;
  finished_ = true;

  if (renderer::Renderer::IsInstanced()) {
    for (auto& renderable : renderables_) {
      if (renderable) renderer::Renderer::Instance().RemoveRenderable(renderable.get());
    }
  }
  renderables_.clear();
  emitters_.clear();
}

}  // namespace vfx
