#pragma once

#include "core/Color.h"

namespace particles {

// One keyed stop in a per-particle colour gradient.
// key ∈ [0, 1] maps to normalised particle age.
struct ColorStop {
  // cppcheck-suppress unusedStructMember
  float key = 0.f;
  // cppcheck-suppress unusedStructMember
  core::Color color{1.f, 1.f, 1.f, 1.f};
};

}  // namespace particles
