#pragma once

#include <cstdint>

namespace physics {

/// Classifies the material/geometry type of a physics body's surface.
/// Used by TireTrackSystem to choose the correct track texture per contact.
enum class SurfaceType : uint8_t {
    kGeneric = 0,  ///< Any geometry not specifically tagged.
    kTerrain = 1,  ///< Height-field terrain (auto-tagged by CreateTerrainBody).
    kRoad    = 2,  ///< Road geometry (tagged by the map serializer via SetBodySurfaceType).
};

/// Number of distinct SurfaceType values.  Keep in sync with the enum.
constexpr int kSurfaceTypeCount = 3;

}  // namespace physics
