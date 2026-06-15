#pragma once

#include <optional>

#include "core/Vec3f.h"

#include <imgui.h>

namespace game { class GameCamera; }
namespace terrain { class TerrainData; }

namespace editor {

// Returns the world-space hit point for a ray cast through mouse_pos.
//
// When terrain_data is non-null, ray-marches against the heightmap and
// refines the intersection with a binary search. When null, falls back
// to the y=0 horizontal plane. Returns std::nullopt when the ray misses
// the terrain bounds, runs parallel to the floor plane, or originates
// below the floor.
std::optional<core::Vec3f> ComputeTerrainHit(
    const game::GameCamera* camera,
    const terrain::TerrainData* terrain_data,
    ImVec2 mouse_pos, ImVec2 image_pos, ImVec2 image_size);

}  // namespace editor
