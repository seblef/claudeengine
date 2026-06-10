#include "game/GameParticleSystem.h"

#include <algorithm>
#include <cmath>
#include <iterator>

#include "core/BBox3.h"
#include "core/Mat4f.h"
#include "core/MathUtils.h"
#include "game/GameSystem.h"
#include "particles/EmbeddedLightDesc.h"
#include "particles/ParticleRenderer.h"
#include "particles/ParticleSystemTemplate.h"
#include "renderer/CircleSpotLight.h"
#include "renderer/OmniLight.h"
#include "renderer/Renderer.h"

namespace game {

namespace {

// Placeholder local AABB. Particles move dynamically so a unit box is used
// for culling and editor selection only.
const core::BBox3 kDefaultBBox{{-1.f, -1.f, -1.f}, {1.f, 1.f, 1.f}};

// Creates the appropriate light subclass from an EmbeddedLightDesc.
// The light is placed at desc.local_offset (adjusted per-frame by
// OnWorldTransformUpdated). Returns nullptr for unsupported types.
std::unique_ptr<renderer::Light> CreateEmbeddedLight(
    const particles::EmbeddedLightDesc& desc) {
  const core::Mat4f offset = core::Mat4f::Translation(desc.local_offset);
  switch (desc.type) {
    case renderer::LightType::kOmni:
      return std::make_unique<renderer::OmniLight>(
          desc.color, desc.intensity_max, desc.radius, offset);

    case renderer::LightType::kCircleSpot:
      return std::make_unique<renderer::CircleSpotLight>(
          desc.color, desc.intensity_max,
          0.2f, 0.4f, 20.f,
          core::Vec3f(0.f, -1.f, 0.f), offset);

    default:
      return nullptr;
  }
}

}  // namespace

GameParticleSystem::GameParticleSystem(particles::ParticleSystemTemplate* tmpl,
                                       abstract::VideoDevice* video)
    : GameObject(GameObjectType::kParticleSystem, kDefaultBBox),
      template_(tmpl),
      video_(video) {
  template_->AddRef();
  const auto& subs = template_->GetSubSystems();
  emitters_.reserve(subs.size());
  std::transform(subs.begin(), subs.end(), std::back_inserter(emitters_),
                 [video](const auto& sub) {
                   return particles::ParticleEmitter(sub, video);
                 });
  for (const auto& light_desc : template_->GetLights()) {
    auto light = CreateEmbeddedLight(light_desc);
    if (light) {
      lights_.push_back(std::move(light));
    }
  }
}

GameParticleSystem::~GameParticleSystem() {
  template_->Release();
}

void GameParticleSystem::OnAddedToScene() {
  particles::ParticleRenderer* pr =
      GameSystem::Instance().GetParticleRenderer();
  for (auto& emitter : emitters_) {
    pr->Register(&emitter);
  }
  for (const auto& light : lights_) {
    renderer::Renderer::Instance().AddRenderable(light.get());
  }
}

void GameParticleSystem::OnRemovedFromScene() {
  particles::ParticleRenderer* pr =
      GameSystem::Instance().GetParticleRenderer();
  for (auto& emitter : emitters_) {
    pr->Unregister(&emitter);
  }
  for (const auto& light : lights_) {
    renderer::Renderer::Instance().RemoveRenderable(light.get());
  }
}

void GameParticleSystem::OnWorldTransformUpdated() {
  const core::Mat4f& wt = GetWorldTransform();
  for (auto& emitter : emitters_) {
    emitter.SetWorldTransform(wt);
  }
  const std::vector<particles::EmbeddedLightDesc>& descs =
      template_->GetLights();
  for (int i = 0; i < static_cast<int>(lights_.size()); ++i) {
    const core::Mat4f offset = core::Mat4f::Translation(descs[i].local_offset);
    lights_[i]->SetWorldMatrix(wt * offset);
  }
}

void GameParticleSystem::Update(float time, float dt) {
  for (auto& emitter : emitters_) {
    emitter.Update(dt);
  }
  const std::vector<particles::EmbeddedLightDesc>& descs =
      template_->GetLights();
  for (int i = 0; i < static_cast<int>(lights_.size()); ++i) {
    const auto& desc = descs[i];
    float t = 0.5f + 0.5f * std::sin(time * desc.flicker_frequency + desc.phase);
    float intensity = core::Lerp(desc.intensity_min, desc.intensity_max, t);
    lights_[i]->SetIntensity(intensity);
  }
}

std::unique_ptr<GameObject> GameParticleSystem::Copy(
    const core::Vec3f& position) const {
  auto clone = std::make_unique<GameParticleSystem>(template_, video_);
  clone->SetName(GetName() + " (copy)");
  core::Mat4f t = GetWorldTransform();
  t(0, 3) = position.x;
  t(1, 3) = position.y;
  t(2, 3) = position.z;
  clone->SetWorldTransform(t);
  return clone;
}

particles::ParticleSystemTemplate* GameParticleSystem::GetTemplate() const {
  return template_;
}

}  // namespace game
