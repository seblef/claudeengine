#pragma once

#include "physics/PhysicsMaterialDesc.h"

namespace track {

// Plain descriptor for a TerrainTile: footprint size and the physics surface
// override applied under it.
//
// friction/restitution in `surface` are applied directly to the tile's own
// static physics body — a low friction models an oil slick, a high
// restitution models a launch ramp — without modifying the terrain beneath.
struct TileDesc {
  // cppcheck-suppress unusedStructMember
  float width  = 4.f;  ///< Local X extent, metres.
  // cppcheck-suppress unusedStructMember
  float length = 4.f;  ///< Local Z extent, metres.
  // cppcheck-suppress unusedStructMember
  physics::PhysicsMaterialDesc surface;  ///< Friction/restitution override.
};

}  // namespace track
