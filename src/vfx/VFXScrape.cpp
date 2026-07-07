#include "vfx/VFXScrape.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "abstract/VideoDevice.h"
#include "audio/ResourceManager.h"
#include "audio/Sound.h"
#include "audio/SoundManager.h"
#include "audio/VirtualSoundInstance.h"
#include "core/Mat4f.h"
#include "particles/ParticleEmitter.h"
#include "particles/ParticleSystemTemplate.h"
#include "renderer/ParticleRenderable.h"
#include "renderer/Renderer.h"

namespace vfx {

namespace {

constexpr int kScrapeSoundPriority = 1;

}  // namespace

VFXScrape::VFXScrape(const physics::ScrapeDesc& desc,
                     audio::SoundManager* sound_manager,
                     audio::ResourceManager* resource_manager,
                     particles::ParticleSystemTemplate* spark_template)
    : desc_(desc),
      sound_manager_(sound_manager),
      resource_manager_(resource_manager),
      spark_template_(spark_template) {}

VFXScrape::~VFXScrape() {
  Stop();
  if (sound_) sound_->Release();
}

void VFXScrape::Play(const core::Vec3f& world_pos, const core::Vec3f& direction) {
  finished_ = false;

  if (renderer::Renderer::IsInstanced()) {
    abstract::VideoDevice* video = renderer::Renderer::Instance().GetVideoDevice();
    if (spark_template_ && !spark_template_->GetSubSystems().empty())
      spark_desc_ = spark_template_->GetSubSystems()[0];
    spark_desc_.direction      = direction;
    spark_desc_.emission_rate  = 0.f;

    const core::Mat4f transform = core::Mat4f::Translation(world_pos);
    spark_emitter_ = std::make_unique<particles::ParticleEmitter>(spark_desc_, video);
    spark_emitter_->SetWorldTransform(transform);
    spark_renderable_ = std::make_unique<renderer::ParticleRenderable>(
        spark_emitter_.get(), transform, /*always_visible=*/false);
    renderer::Renderer::Instance().AddRenderable(spark_renderable_.get());
  }

  if (resource_manager_ && !desc_.screech_sound.empty())
    sound_ = resource_manager_->LoadSound(desc_.screech_sound);
  if (sound_manager_ && sound_) {
    instance_ = sound_manager_->PlaySound(sound_, world_pos, /*loop=*/true,
                                          kScrapeSoundPriority, /*gain=*/0.f);
  }
}

void VFXScrape::Update(float dt) {
  if (finished_ || !spark_emitter_) return;
  spark_emitter_->Update(dt);
  spark_emitter_->UploadToGPU();
}

void VFXScrape::UpdateContact(const core::Vec3f& world_point,
                              const core::Vec3f& surface_normal,
                              const core::Vec3f& relative_velocity) {
  if (finished_) return;

  const float speed     = relative_velocity.Length();
  const float intensity = ComputeIntensity(speed, desc_);

  const core::Vec3f transform_pos = world_point;
  if (spark_emitter_) {
    spark_desc_.direction     = ReflectDirection(relative_velocity, surface_normal.Normalized());
    spark_desc_.emission_rate = desc_.base_emission_rate * intensity;

    const core::Mat4f transform = core::Mat4f::Translation(transform_pos);
    spark_emitter_->SetWorldTransform(transform);
    if (spark_renderable_) spark_renderable_->SetWorldMatrix(transform);
  }

  if (instance_) {
    instance_->SetPosition(transform_pos);
    instance_->SetGain(desc_.base_gain * intensity);
  }
}

void VFXScrape::Stop() {
  if (finished_) return;
  finished_ = true;

  if (instance_) {
    instance_->Stop();
    instance_ = nullptr;
  }

  if (spark_renderable_) {
    renderer::Renderer::Instance().RemoveRenderable(spark_renderable_.get());
    spark_renderable_.reset();
  }
  spark_emitter_.reset();
}

// static
float VFXScrape::ComputeIntensity(float speed, const physics::ScrapeDesc& desc) {
  const float span = desc.max_speed - desc.min_speed;
  if (span <= 0.f) return speed >= desc.min_speed ? 1.f : 0.f;
  return std::clamp((speed - desc.min_speed) / span, 0.f, 1.f);
}

// static
core::Vec3f VFXScrape::ReflectDirection(const core::Vec3f& incoming,
                                        const core::Vec3f& normal) {
  const float len = incoming.Length();
  if (len < 1e-6f) return normal;

  const core::Vec3f dir = incoming * (1.f / len);
  const core::Vec3f reflected = dir - normal * (2.f * dir.Dot(normal));
  const float reflected_len = reflected.Length();
  return reflected_len > 1e-6f ? reflected * (1.f / reflected_len) : normal;
}

}  // namespace vfx
