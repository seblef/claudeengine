#pragma once

#include <memory>
#include <vector>

#include "core/Singleton.h"
#include "core/Vec3f.h"
#include "vfx/IVFXEffect.h"

namespace vfx {

// Singleton coordination layer for composited visual effects.
//
// The particle system only knows about raw emitters; VFXSystem owns the
// lifetime of higher-level, multi-element effects (an explosion is a burst +
// flash + shockwave + shake). Spawn() takes ownership of an effect, plays it
// immediately, and ticks it from Update(dt) each frame until IsFinished()
// returns true, at which point it is destroyed.
//
// Lifecycle: new VFXSystem() -> Update(dt) once per frame -> Shutdown().
class VFXSystem : public core::Singleton<VFXSystem> {
 public:
  VFXSystem() = default;
  ~VFXSystem() = default;

  // Takes ownership of effect, plays it at world_pos oriented along
  // direction, and returns a non-owning pointer valid until the effect
  // finishes and is reaped by a later Update() call.
  IVFXEffect* Spawn(std::unique_ptr<IVFXEffect> effect,
                     const core::Vec3f& world_pos,
                     const core::Vec3f& direction);

  // Advances every active effect by dt seconds and destroys finished ones.
  void Update(float dt);

  // Returns the number of effects not yet finished.
  [[nodiscard]] size_t GetActiveEffectCount() const { return effects_.size(); }

 private:
  // cppcheck-suppress unusedStructMember
  std::vector<std::unique_ptr<IVFXEffect>> effects_;
};

}  // namespace vfx
