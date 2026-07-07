#include "vfx/VFXSystem.h"

#include <algorithm>
#include <utility>

namespace vfx {

IVFXEffect* VFXSystem::Spawn(std::unique_ptr<IVFXEffect> effect,
                              const core::Vec3f& world_pos,
                              const core::Vec3f& direction) {
  effects_.push_back(std::move(effect));
  IVFXEffect* raw = effects_.back().get();
  raw->Play(world_pos, direction);
  return raw;
}

void VFXSystem::Update(float dt) {
  for (auto& effect : effects_) {
    effect->Update(dt);
  }
  effects_.erase(
      std::remove_if(effects_.begin(), effects_.end(),
                      [](const std::unique_ptr<IVFXEffect>& effect) {
                        return effect->IsFinished();
                      }),
      effects_.end());
}

}  // namespace vfx
