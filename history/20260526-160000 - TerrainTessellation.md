# Terrain Tessellation Shaders — Issue #289

**Date:** 2026-05-26
**Branch:** feat/terrain-tessellation-shaders-289

---

## What was implemented

Hardware tessellation support for the terrain G-buffer pass. Close LOD levels
(default: LOD 0–2) are now rendered through a VS + TESC + TESE + FS pipeline
that adds silhouette-smoothing geometry; farther LOD levels fall back to the
existing VS + FS triangle path.

### New shader files

| File | Purpose |
|------|---------|
| `data/shaders/glsl/terrain/terrain_tesc.glsl` | Tessellation-control shader: computes per-edge tessellation levels from camera distance |
| `data/shaders/glsl/terrain/terrain_tese.glsl` | Tessellation-evaluation shader: bilinear interpolation + heightmap displacement + Phong smoothing |

### Modified source files

| File | Change |
|------|--------|
| `src/abstract/PrimitiveType.h` | Added `kPatch4` topology for GL_PATCHES/4 |
| `src/abstract/Shader.h` | Added `HasTessellation()` and `ActivateTess()` virtual methods with default no-ops |
| `src/gldevices/GLShader.h` | Added `tess_program_id_`, overrides for `HasTessellation()` and `ActivateTess()` |
| `src/gldevices/GLShader.cpp` | Optional TESC/TESE stage loading; extracted `LinkProgram()` helper |
| `src/gldevices/GLVideoDevice.cpp` | Handle `kPatch4` → `GL_PATCHES` + `glPatchParameteri(GL_PATCH_VERTICES, 4)` |
| `src/terrain/TerrainPatchInfos.h` | `pad1_` → `tess_falloff_dist`, `pad2_` → `max_tess` (same layout) |
| `data/shaders/glsl/uniforms/terrain_patch_infos.glsl` | Same rename — no std140 layout change |
| `src/terrain/TerrainPatchMesh.h/.cpp` | Added `tess_ibo_`, `BindTess()`, `GetTessIndexCount()` for quad-patch IBO |
| `src/terrain/TerrainRenderer.h/.cpp` | `SetTessellationEnabled`, `SetTessellationFalloffDistance`, `SetMaxTessFactor`, `SetMaxTessLod`; dual render loop |

---

## Decisions and rationale

### One shader object, two programs

`GLShader` automatically detects tessellation stages: if `<name>_tesc.glsl` and
`<name>_tese.glsl` exist alongside the VS/FS files, a second GPU program is
compiled and stored in `tess_program_id_`. `Activate()` binds the plain VS/FS
program; `ActivateTess()` binds the tessellated one.

This keeps the API to a single `CreateShader("terrain/terrain")` call and lets
`HasTessellation()` drive switching at runtime without needing a second shader
registration.

### Two render loops in TerrainRenderer::Render()

LOD levels ≤ `max_tess_lod_` (default 2) are rendered first with
`ActivateTess()` / `kPatch4` / the quad-patch IBO. Higher LOD levels use the
original `Activate()` / `kTriangleList` / triangle IBO path. Both loops reuse
the same UBO bind (done once before the loops).

This avoids the overhead of running the tessellation pipeline for far-away
patches where the extra geometry would be invisible.

### Quad-patch IBO (4 indices per quad, not 6)

A second index buffer was added to `TerrainPatchMesh`. Control-point order:
`v00(u=0,v=0)`, `v10(u=1,v=0)`, `v11(u=1,v=1)`, `v01(u=0,v=1)`. This aligns
with OpenGL's convention for `layout(quads)` TES coordinate mapping and with
the edge-numbering assumed by `gl_TessLevelOuter[0..3]`.

### Tessellation parameters via the existing per-patch UBO

`tess_falloff_dist` and `max_tess` replaced the two formerly-unused float
padding fields in `TerrainPatchInfos` / `terrain_patch_infos.glsl`. The std140
layout is unchanged (same size, same offsets for every other field). Filling
these per-patch is slightly redundant since they are global parameters, but it
avoids a second UBO and keeps all terrain shader inputs in one block.

### Phong tessellation

The TES applies bilinear Phong tessellation: for each of the 4 control points,
it computes the projection correction `dot(pos - ctrl_pos, ctrl_normal) * ctrl_normal`,
blends with bilinear weights, and mixes the result at `kPhongAlpha = 0.75`.
This smooths silhouettes without requiring geometry normals to be pre-baked.

---

## Output to keep in mind for future features

- `SetMaxTessFactor(float)` is exposed but has no IMGUI widget yet; the editor
  pass (#296 or later) should hook into it.
- The TES samples the heightmap at the interpolated UV. If a per-vertex normal
  map (TerrainNormalMap, #288) is later used in the TESE, it can replace the
  bilinearly-interpolated normal coming from the TCS for higher fidelity.
- `tess_falloff_dist_` defaults to 500 m. This is a reasonable default for
  large outdoor terrains but should be exposed in `config.yaml` eventually.
- If the tessellation shader fails to compile (e.g., driver doesn't support
  OpenGL 4.6 tessellation), `HasTessellation()` returns false and the renderer
  silently falls back to the triangle path — no crash.

---

## Skills and instructions used

- `impl-issue` skill
- `src/CLAUDE.md`: one class per file, Google C++ style, include root is `src/`
- `data/shaders/glsl/CLAUDE.md`: custom `#include` supported; uniforms via include
- `src/terrain/CLAUDE.md`: allowed deps are `core` and `abstract` only
- Root `CLAUDE.md`: history file, cpplint before commit, conventional commit message
