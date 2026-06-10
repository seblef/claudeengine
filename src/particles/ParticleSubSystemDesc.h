#pragma once

#include <string>

#include "core/Color.h"
#include "core/Vec3f.h"
#include "particles/EmitterShape.h"
#include "particles/ParticleAnimationMode.h"
#include "particles/ParticleBlendMode.h"

namespace particles {

// Describes one particle sub-system within a ParticleSystemTemplate.
//
// A single template may contain several sub-systems (e.g. fire body + smoke).
// Each sub-system owns its emitter settings, sprite-sheet animation, and
// per-particle lifetime parameters.
struct ParticleSubSystemDesc {
  // cppcheck-suppress unusedStructMember
  std::string name;

  ParticleBlendMode blend_mode = ParticleBlendMode::kAdditive;

  // Only meaningful when blend_mode == kAlphaBlend.
  bool lit = false;

  // Path relative to data/textures/.
  // cppcheck-suppress unusedStructMember
  std::string texture;

  // Sprite-sheet subdivisions (1×1 = no animation).
  int sprite_cols = 1;
  int sprite_rows = 1;

  // Frames per second for sprite animation. 0 = pick a random frame per particle.
  float animation_fps = 0.f;

  ParticleAnimationMode animation_mode = ParticleAnimationMode::kSequential;

  EmitterShape emitter_shape  = EmitterShape::kPoint;
  float        emitter_radius = 0.f;

  // Particles emitted per second.
  float emission_rate = 10.f;
  int   max_particles = 100;

  bool  looping  = true;
  // Ignored when looping is true.
  float duration = 1.f;

  float lifetime_min = 1.f;
  float lifetime_max = 2.f;

  float speed_min = 1.f;
  float speed_max = 3.f;

  // cppcheck-suppress unusedStructMember
  core::Vec3f direction{0.f, 1.f, 0.f};

  // Cone half-angle in degrees around direction.
  float spread = 30.f;

  // Downward acceleration (positive = falls, negative = rises).
  float gravity = 0.f;

  float size_start_min = 0.1f;
  float size_start_max = 0.3f;
  float size_end_min   = 0.f;
  float size_end_max   = 0.1f;

  // cppcheck-suppress unusedStructMember
  core::Color color_start{1.f, 1.f, 1.f, 1.f};
  // cppcheck-suppress unusedStructMember
  core::Color color_end{1.f, 1.f, 1.f, 0.f};
};

}  // namespace particles
