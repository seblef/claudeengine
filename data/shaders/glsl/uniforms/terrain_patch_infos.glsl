// TerrainPatchInfosBlock — must match terrain::TerrainPatchInfos exactly (48 bytes, 3 float4s).
// std140 offsets:
//   [  0] patch_origin       vec2  ( 8 B — world XZ of patch corner)
//   [  8] patch_scale        float ( 4 B — world metres per mesh unit)
//   [ 12] lod_level          int   ( 4 B)
//   [ 16] morph_factor       float ( 4 B — CDLOD blend [0,1] toward next coarser LOD)
//   [ 20] heightmap_scale    float ( 4 B — max_height - min_height)
//   [ 24] heightmap_offset   float ( 4 B — min_height)
//   [ 28] tpi_pad0_          float ( 4 B)
//   [ 32] inv_terrain_world  vec2  ( 8 B — 1/world_width, 1/world_height for UV)
//   [ 40] tess_falloff_dist  float ( 4 B — distance in metres where tess factor → 1)
//   [ 44] max_tess           float ( 4 B — maximum tessellation factor at dist=0)
// Note: tpi_pad0_ is prefixed with tpi_ to avoid clashing with SceneInfosBlock's
// pad0_/pad1_/pad2_ — nameless UBO members share the global GLSL namespace.
layout(std140, binding = 6) uniform TerrainPatchInfosBlock {
    vec2  patch_origin;
    float patch_scale;
    int   lod_level;
    float morph_factor;
    float heightmap_scale;
    float heightmap_offset;
    float tpi_pad0_;
    vec2  inv_terrain_world;
    float tess_falloff_dist;
    float max_tess;
};
