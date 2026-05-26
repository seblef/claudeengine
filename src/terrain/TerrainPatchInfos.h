#pragma once

#include <cstddef>

#include "core/Vec2f.h"
#include "core/Vec4f.h"

namespace terrain {

// GPU constant buffer layout for per-patch terrain data (UBO slot 6).
// Matches layout(std140) exactly — do not reorder fields.
//
// std140 offsets:
//   [  0] patch_origin         vec2  ( 8 B — world XZ of patch corner)
//   [  8] patch_scale          float ( 4 B — world metres per mesh unit)
//   [ 12] lod_level            int   ( 4 B)
//   [ 16] morph_factor         float ( 4 B — CDLOD blend [0,1] toward next coarser LOD)
//   [ 20] heightmap_scale      float ( 4 B — max_height - min_height)
//   [ 24] heightmap_offset     float ( 4 B — min_height)
//   [ 28] tpi_pad0_            float ( 4 B) — GLSL name avoids clash with SceneInfosBlock
//   [ 32] inv_terrain_world    vec2  ( 8 B — 1/world_width, 1/world_height for UV)
//   [ 40] tess_falloff_dist    float ( 4 B — distance at which tessellation factor → 1)
//   [ 44] max_tess             float ( 4 B — maximum tessellation factor at dist=0)
//   [ 48] tiling               vec4  (16 B — per-layer UV tiling, x=layer0 … w=layer3)
//   [ 64] triplanar_threshold  float ( 4 B — |normal.y| below which triplanar activates)
//   [ 68] use_macro_texture    int   ( 4 B — 1 if slot 10 carries a macro texture)
//   [ 72] tpi_pad1_            float ( 4 B)
//   [ 76] tpi_pad2_            float ( 4 B)
//   [ 80] end                  5 float4s total
struct TerrainPatchInfos {
  core::Vec2f patch_origin;           // world XZ of the patch's (0,0) corner
  float       patch_scale;            // world metres per mesh-grid unit
  int         lod_level;              // LOD level (0 = finest)
  float       morph_factor;           // CDLOD blend toward next coarser LOD
  float       heightmap_scale;        // max_height - min_height (world range)
  float       heightmap_offset;       // min_height (world base)
  float       tpi_pad0_          = 0.f;
  core::Vec2f inv_terrain_world;      // (1/world_width, 1/world_height)
  float       tess_falloff_dist  = 500.f;  // distance in metres where tess factor reaches 1
  float       max_tess           = 8.f;    // tessellation factor at camera distance = 0
  core::Vec4f tiling             = {1.f, 1.f, 1.f, 1.f};  // per-layer UV tiling factors
  float       triplanar_threshold = 0.5f;  // |normal.y| threshold for triplanar activation
  int         use_macro_texture  = 0;      // 1 if macro texture is bound to slot 10
  float       tpi_pad1_          = 0.f;
  float       tpi_pad2_          = 0.f;
};

static_assert(sizeof(TerrainPatchInfos) == 80);
static_assert(offsetof(TerrainPatchInfos, patch_scale)          ==  8);
static_assert(offsetof(TerrainPatchInfos, lod_level)            == 12);
static_assert(offsetof(TerrainPatchInfos, morph_factor)         == 16);
static_assert(offsetof(TerrainPatchInfos, heightmap_scale)      == 20);
static_assert(offsetof(TerrainPatchInfos, heightmap_offset)     == 24);
static_assert(offsetof(TerrainPatchInfos, inv_terrain_world)    == 32);
static_assert(offsetof(TerrainPatchInfos, tiling)               == 48);
static_assert(offsetof(TerrainPatchInfos, triplanar_threshold)  == 64);
static_assert(offsetof(TerrainPatchInfos, use_macro_texture)    == 68);

}  // namespace terrain
