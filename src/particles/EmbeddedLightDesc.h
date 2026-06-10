#pragma once

#include "core/Color.h"
#include "core/Vec3f.h"
#include "renderer/Light.h"

namespace particles {

// Describes a dynamic light embedded in a particle system.
//
// Supports kOmni and kCircleSpot only. The light flickers around a base
// intensity range at flicker_frequency Hz with flicker_amplitude depth.
// phase is a per-instance random offset assigned at construction time to
// prevent all instances from pulsing in sync.
struct EmbeddedLightDesc {
  // kOmni or kCircleSpot only.
  renderer::LightType type = renderer::LightType::kOmni;

  // cppcheck-suppress unusedStructMember
  core::Color color{1.f, 1.f, 1.f};

  float intensity_min = 1.f;
  float intensity_max = 2.f;
  float radius        = 5.f;

  // Position relative to the particle system's local origin.
  // cppcheck-suppress unusedStructMember
  core::Vec3f local_offset{0.f, 0.f, 0.f};

  // Flicker rate in Hz.
  float flicker_frequency = 5.f;

  // Flicker depth in [0, 1]: fraction of intensity range modulated per cycle.
  float flicker_amplitude = 0.2f;

  // Per-instance random phase offset assigned at construction time.
  float phase = 0.f;
};

}  // namespace particles
