#pragma once

#include "core/Vec3f.h"

namespace vfx {

// Interface for a self-contained, self-expiring composited visual effect
// (e.g. an explosion combining a particle burst, a light flash, and a screen
// shake).
//
// VFXSystem owns instances behind this interface: Play() starts the effect
// once, Update() advances it every frame, and IsFinished() is polled so the
// instance can be reaped once it has run its course.
class IVFXEffect {
 public:
  virtual ~IVFXEffect() = default;

  IVFXEffect(const IVFXEffect&)            = delete;
  IVFXEffect& operator=(const IVFXEffect&) = delete;

  // Starts the effect at world_pos, oriented along direction (normalized).
  virtual void Play(const core::Vec3f& world_pos, const core::Vec3f& direction) = 0;

  // Advances the effect by dt seconds. Called once per frame by VFXSystem.
  virtual void Update(float dt) = 0;

  // Returns true once the effect has completed and can be destroyed.
  [[nodiscard]] virtual bool IsFinished() const = 0;

 protected:
  IVFXEffect() = default;
};

}  // namespace vfx
