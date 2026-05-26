#pragma once

#include <cstddef>

#include "core/Vec2f.h"

namespace terrain {

// GPU constant buffer layout for per-patch terrain data (UBO slot 6).
// Matches layout(std140) exactly — do not reorder fields.
//
// std140 offsets:
//   [  0] patch_origin       vec2  ( 8 B — world XZ of patch corner)
//   [  8] patch_scale        float ( 4 B — world metres per mesh unit)
//   [ 12] lod_level          int   ( 4 B)
//   [ 16] morph_factor       float ( 4 B — CDLOD blend [0,1] toward next coarser LOD)
//   [ 20] heightmap_scale    float ( 4 B — max_height - min_height)
//   [ 24] heightmap_offset   float ( 4 B — min_height)
//   [ 28] tpi_pad0_          float ( 4 B) — GLSL name avoids clash with SceneInfosBlock
//   [ 32] inv_terrain_world  vec2  ( 8 B — 1/world_width, 1/world_height for UV)
//   [ 40] tpi_pad1_          float ( 4 B)
//   [ 44] tpi_pad2_          float ( 4 B)
//   [ 48] end                3 float4s total
struct TerrainPatchInfos {
  core::Vec2f patch_origin;       // world XZ of the patch's (0,0) corner
  float       patch_scale;        // world metres per mesh-grid unit
  int         lod_level;          // LOD level (0 = finest)
  float       morph_factor;       // CDLOD blend toward next coarser LOD
  float       heightmap_scale;    // max_height - min_height (world range)
  float       heightmap_offset;   // min_height (world base)
  float       pad0_     = 0.f;
  core::Vec2f inv_terrain_world;  // (1/world_width, 1/world_height)
  float       pad1_     = 0.f;
  float       pad2_     = 0.f;
};

static_assert(sizeof(TerrainPatchInfos) == 48);
static_assert(offsetof(TerrainPatchInfos, patch_scale)       ==  8);
static_assert(offsetof(TerrainPatchInfos, lod_level)         == 12);
static_assert(offsetof(TerrainPatchInfos, morph_factor)      == 16);
static_assert(offsetof(TerrainPatchInfos, heightmap_scale)   == 20);
static_assert(offsetof(TerrainPatchInfos, heightmap_offset)  == 24);
static_assert(offsetof(TerrainPatchInfos, inv_terrain_world) == 32);

}  // namespace terrain
