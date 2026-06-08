// TerrainPatchInfosBlock — must match terrain::TerrainPatchInfos exactly (80 bytes, 5 float4s).
// std140 offsets:
//   [  0] patch_origin         vec2  ( 8 B — world XZ of patch corner)
//   [  8] patch_scale          float ( 4 B — world metres per mesh unit)
//   [ 12] lod_level            int   ( 4 B)
//   [ 16] morph_factor         float ( 4 B — CDLOD blend [0,1] toward next coarser LOD)
//   [ 20] heightmap_scale      float ( 4 B — max_height - min_height)
//   [ 24] heightmap_offset     float ( 4 B — min_height)
//   [ 28] tpi_pad0_            float ( 4 B)
//   [ 32] inv_terrain_world    vec2  ( 8 B — 1/world_width, 1/world_height for UV)
//   [ 40] tess_falloff_dist    float ( 4 B — distance in metres where tess factor → 1)
//   [ 44] max_tess             float ( 4 B — maximum tessellation factor at dist=0)
//   [ 48] tiling               vec4  (16 B — per-layer UV tiling, x=layer0 … w=layer3)
//   [ 64] triplanar_threshold  float ( 4 B — |normal.y| below which triplanar activates)
//   [ 68] use_macro_texture    int   ( 4 B — 1 if slot 10 carries a macro texture)
//   [ 72] use_terrain_normal_map int  ( 4 B — 1 if slot 12 carries the terrain normal map)
//   [ 76] tpi_pad2_            float ( 4 B)
// Note: tpi_pad0_/tpi_pad1_/tpi_pad2_ prefixes avoid clashing with SceneInfosBlock's
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
    vec4  tiling;              // per-layer UV tiling: x=layer0, y=layer1, z=layer2, w=layer3
    float triplanar_threshold; // |world_normal.y| below which triplanar mapping activates
    int   use_macro_texture;        // 1 if u_macro_texture (slot 10) is valid
    int   tpi_pad1_;
    float tpi_pad2_;
};
