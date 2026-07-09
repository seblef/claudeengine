#pragma once

#include <string>

#include "physics/VehicleDesc.h"
#include "vfx/VFXElectricityDesc.h"
#include "vfx/VFXExplosionDesc.h"
#include "vfx/VFXFireDesc.h"

namespace vfx {

// Discriminates which IVFXEffect subclass a .vfx.yaml resource file authors.
enum class VFXEffectType {
  kExplosion,
  kElectricity,
  kFire,
  kScrape,
};

// Top-level, YAML-serialisable descriptor for a .vfx.yaml resource file (see
// editor::VFXPanel). Every parameter block is always present, not just the
// active one, so switching effect_type in the editor never discards
// previously authored values for the other effect types.
//
// physics::ScrapeDesc is reused as-is for kScrape rather than duplicated: it
// already carries exactly the standalone, YAML-serialisable tunables
// vfx::VFXScrape's constructor takes.
struct VFXDesc {
  VFXEffectType effect_type = VFXEffectType::kExplosion;

  // cppcheck-suppress unusedStructMember
  VFXExplosionDesc    explosion;
  // cppcheck-suppress unusedStructMember
  VFXElectricityDesc  electricity;
  // cppcheck-suppress unusedStructMember
  VFXFireDesc         fire;
  // cppcheck-suppress unusedStructMember
  physics::ScrapeDesc scrape;

  // Optional sound asset stem (data/sounds/*.sound.yaml basename) played
  // alongside the effect. Ignored for kScrape, which carries its own
  // screech_sound field in `scrape`.
  // cppcheck-suppress unusedStructMember
  std::string sound;
};

}  // namespace vfx
