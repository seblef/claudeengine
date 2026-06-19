#include "game/GameSoundEmitter.h"

#include <string>

#include <loguru.hpp>

#include "audio/ResourceManager.h"
#include "audio/Sound.h"
#include "audio/SoundManager.h"
#include "audio/VirtualSoundInstance.h"
#include "core/BBox3.h"
#include "core/Mat4f.h"
#include "core/Vec3f.h"
#include "game/GameObjectType.h"

namespace game {

namespace {

// Unit-cube AABB used for editor selection; sound emitters have no geometry.
const core::BBox3 kEmitterBBox{{-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}};

}  // namespace

GameSoundEmitter::GameSoundEmitter(const std::string& sound_name,
                                   audio::SoundManager* sound_manager,
                                   audio::ResourceManager* resource_manager,
                                   float volume_scale)
    : GameObject(GameObjectType::kSoundEmitter, kEmitterBBox),
      sound_name_(sound_name),
      sound_manager_(sound_manager),
      resource_manager_(resource_manager),
      volume_scale_(volume_scale) {
  if (resource_manager_ && !sound_name_.empty()) {
    sound_ = resource_manager_->LoadSound(sound_name_);
    if (!sound_) {
      LOG_F(WARNING, "GameSoundEmitter: sound '%s' failed to load",
            sound_name_.c_str());
    }
  }
}

GameSoundEmitter::~GameSoundEmitter() {
  if (sound_) {
    sound_->Release();
  }
}

void GameSoundEmitter::OnAddedToScene() {
  if (!sound_manager_ || !sound_) return;

  const core::Mat4f& wt = GetWorldTransform();
  const core::Vec3f  pos{wt(0, 3), wt(1, 3), wt(2, 3)};

  instance_ = sound_manager_->PlaySound(sound_, pos,
                                        /*loop=*/true,
                                        /*priority=*/0,
                                        volume_scale_);
  if (!instance_) {
    LOG_F(WARNING, "GameSoundEmitter: SoundManager::PlaySound returned null "
                   "for '%s'", sound_name_.c_str());
  }
}

void GameSoundEmitter::OnRemovedFromScene() {
  if (instance_) {
    instance_->Stop();
    instance_ = nullptr;
  }
}

void GameSoundEmitter::OnWorldTransformUpdated() {
  if (!instance_) return;

  const core::Mat4f& wt = GetWorldTransform();
  instance_->SetPosition({wt(0, 3), wt(1, 3), wt(2, 3)});
}

float GameSoundEmitter::GetMinDistance() const {
  return sound_ ? sound_->GetDesc().min_distance : 1.0f;
}

float GameSoundEmitter::GetMaxDistance() const {
  return sound_ ? sound_->GetDesc().max_distance : 100.0f;
}

std::unique_ptr<GameObject> GameSoundEmitter::Copy(
    const core::Vec3f& position) const {
  auto clone = std::make_unique<GameSoundEmitter>(
      sound_name_, sound_manager_, resource_manager_, volume_scale_);
  clone->SetName(GetName());
  core::Mat4f t = GetWorldTransform();
  t(0, 3) = position.x;
  t(1, 3) = position.y;
  t(2, 3) = position.z;
  clone->SetWorldTransform(t);
  return clone;
}

}  // namespace game
