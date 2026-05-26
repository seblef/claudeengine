# TerrainRenderer — G-Buffer Pass Renderer

**Date:** 2026-05-26  
**Issue:** #286  
**Branch:** feat/terrain-renderer-286

---

## Summary

Implements `terrain::TerrainRenderer`, a singleton G-buffer pass renderer that
drives the CDLOD terrain system from GPU initialisation through per-frame draw
calls. Adds the supporting infrastructure required to upload raw CPU data as a
GPU texture and to wire the terrain into `renderer::Renderer`'s geometry pass.

---

## What was changed

### New files

| File | Purpose |
|------|---------|
| `src/abstract/RawTexture.h` | Minimal GPU-texture interface (no resource registry). `Bind(slot)` only. |
| `src/gldevices/GLRawTexture.h/.cpp` | OpenGL GL_R16 texture from CPU uint16 data. |
| `src/terrain/TerrainPatchInfos.h` | Per-patch constant-buffer struct (slot 6, 48 bytes / 3 float4s). |
| `src/terrain/TerrainRenderer.h/.cpp` | Singleton renderer: Init, SetTriangleBudget, Render. |
| `data/shaders/glsl/uniforms/terrain_patch_infos.glsl` | GLSL UBO binding 6 declaration. |
| `data/shaders/glsl/terrain_gbuffer_vs.glsl` | Terrain vertex shader: CDLOD morph + heightmap sampling + central-difference normals. |
| `data/shaders/glsl/terrain_gbuffer_ps.glsl` | Terrain fragment shader: writes albedo/normal/specular to G-buffer MRTs. |

### Modified files

| File | Change |
|------|--------|
| `src/abstract/VideoDevice.h` | Added `CreateHeightmapTexture(w, h, data*)` pure-virtual factory. |
| `src/gldevices/GLVideoDevice.h/.cpp` | Implemented `CreateHeightmapTexture` via `GLRawTexture`. |
| `src/terrain/TerrainData.h` | Added `GetMetersPerTexel()`, `GetMinHeight()`, `GetMaxHeight()` getters. |
| `src/terrain/CMakeLists.txt` | Added `TerrainRenderer.cpp`. |
| `src/terrain/CLAUDE.md` | Documented TerrainPatchInfos and TerrainRenderer. |
| `src/renderer/Renderer.h/.cpp` | Added `terrain_renderer_` member + `SetTerrainRenderer()` + geometry-pass call. |
| `src/renderer/CMakeLists.txt` | Added `terrain` to renderer link dependencies. |
| `src/gldevices/CMakeLists.txt` | Added `GLRawTexture.cpp`. |

---

## Design decisions

### `abstract::RawTexture` instead of `abstract::Texture`

`abstract::Texture` inherits from `core::Resource<string, Texture>`, which
registers every instance in a static map keyed by filename. CPU-uploaded
textures (heightmaps) have no filename and must not participate in that
registry. A separate `RawTexture` base (just `Bind(slot)`, virtual destructor)
avoids the problem entirely and fits the engine's "unique_ptr ownership"
pattern for other non-registry GPU objects (ConstantBuffer, RenderTarget, etc.).

### GL_R16 for the heightmap

The raw heightmap data is `uint16_t` (0 → min_height, 65535 → max_height).
`GL_R16` with `GL_UNSIGNED_SHORT` uploads this natively and exposes `[0, 1]`
in shaders via a `sampler2D`. The inverse mapping is `height = offset + r * scale`,
which is implemented in `SampleHeight()` in the vertex shader.

Using `GL_R16F` (float16) would have required a CPU-side uint16 → float16
conversion, adding complexity for no benefit. `GL_R16` is the correct internal
format for unsigned-normalised 16-bit heightmaps in OpenGL 4.6.

### Per-patch constant buffer (slot 6)

Layout (std140, 48 bytes):
```
vec2  patch_origin     — world XZ of the patch's mesh (0,0) vertex
float patch_scale      — world metres per mesh-grid unit
int   lod_level        — LOD level (0 = finest)
float morph_factor     — CDLOD blend toward next coarser LOD [0,1]
float heightmap_scale  — max_height - min_height
float heightmap_offset — min_height
float pad0_
vec2  inv_terrain_world — (1/world_width, 1/world_height) for UV computation
float pad1_
float pad2_
```

`inv_terrain_world` was added on top of the issue spec to let the vertex shader
compute heightmap UV from world XZ without knowing terrain dimensions from
another source.

### CDLOD morph in the vertex shader

Odd-indexed mesh vertices are snapped to the nearest even-indexed (coarser) vertex
and blended by `morph_factor`:
```glsl
vec2 snapped = floor(in_position / 2.0) * 2.0;
vec2 local   = mix(in_position, snapped, morph_factor);
```
This prevents popping at LOD boundaries and matches the standard CDLOD technique.

### Central-difference normals

Normals are computed in the vertex shader by sampling the heightmap at ±1 texel
in X and Z. One texel in UV space at the current LOD is:
```glsl
float mpt  = patch_scale / float(1 << lod_level);
vec2  step = inv_terrain_world * mpt;
```
The Y-up normal formula is:
```glsl
v_normal = normalize(vec3(h_l - h_r, 2.0 * mpt, h_b - h_f));
```
This avoids extra uniform slots and keeps the shader self-contained.

### Renderer integration

`terrain_renderer_` is a non-owning pointer in `renderer::Renderer`. The
terrain is registered via `SetTerrainRenderer(ptr)` and called in the geometry
pass after `MeshRenderer::Render()`. The terrain system is optional: if no
terrain is registered, the geometry pass proceeds unchanged.

---

## Constant buffer slot map (updated)

| Slot | Contents |
|------|---------|
| 1 | RenderableInfos (per-object world matrix) |
| 2 | SceneInfos (camera, time) |
| 3 | MaterialInfos (colours, shininess) |
| 5 | CSMInfos (cascade VP matrices) |
| 6 | TerrainPatchInfos (new — per-patch terrain data) |

---

## Output for next features

- **Terrain texturing**: add a diffuse texture array (grass, rock, snow) to the
  PS and blend by slope/height. Requires a new material system for terrain or
  extra samplers in `terrain_gbuffer_ps.glsl`.
- **Normal map**: pre-compute a normal map texture at Init time for smoother
  normals across LOD transitions, replacing per-vertex central differences.
- **Editor integration**: expose `SetTriangleBudget` and `Init` from an editor
  panel so artists can configure the terrain LOD live.
- **Multiple terrains**: `terrain_renderer_` is currently a single pointer.
  To support multiple terrains, change to a `std::vector<TerrainRenderer*>`.
- **Slot 4 is free** (slots 1–3, 5, 6 are taken).

---

## Skills and instructions applied

- `src/CLAUDE.md`: one class per file, Google C++ style, include root `src/`
- `src/terrain/CLAUDE.md`: no gldevices/renderer includes in terrain; allowed only core, abstract
- `data/shaders/glsl/CLAUDE.md`: vertex+pixel pair with `_vs`/`_ps` suffix, `#include` for uniforms and layouts
