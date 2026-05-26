# Terrain Splatmap Shaders — Issue #291

## Summary

Replaced the flat-shading placeholder in `terrain_ps.glsl` with a full 4-layer splatmap blending shader, including triplanar mapping for steep slopes, tangent-space normal blending, and optional macro texture support.

Extended `TerrainPatchInfos` (and its GLSL mirror) with the new per-material uniforms, and wired up material texture binding in `TerrainRenderer`.

## Changes

| File | Change |
|------|--------|
| `data/shaders/glsl/terrain/terrain_ps.glsl` | Full rewrite: splatmap blending, triplanar mapping, normal blending, macro texture |
| `data/shaders/glsl/uniforms/terrain_patch_infos.glsl` | Added `tiling` (vec4), `triplanar_threshold` (float), `use_macro_texture` (int), two pads |
| `src/terrain/TerrainPatchInfos.h` | Same additions; struct grows from 48 → 80 bytes (3 → 5 float4s) |
| `src/terrain/TerrainRenderer.h` | Added `SetMaterial()`, `SetMacroTexture()`, `SetTriplanarThreshold()`; `BindMaterialTextures()` private helper |
| `src/terrain/TerrainRenderer.cpp` | Implements material binding and tiling fill in `FillPatchInfos()` |

## Decisions and rationale

### Texture slot layout

The heightmap lives at slot 0 in the vertex and tessellation-evaluation shaders.  OpenGL texture bindings are program-global (not per-stage), so the splatmap cannot also be at slot 0.  The slot map chosen:

```
0  — heightmap  (VS / TESE)
1  — splatmap   (PS)
2–5 — layer albedos  (PS, layers 0–3)
6–9 — layer normals  (PS, layers 0–3)
10 — macro texture   (PS, optional)
```

This differs from the slot numbering suggested in the issue (where splatmap was at 0) but is required to avoid a heightmap/splatmap alias.

### `vec4 tiling` instead of `float tiling[4]`

In std140 layout, `float arr[N]` pads each element to 16 bytes, making `float[4]` 64 bytes in GLSL but only 16 bytes in C++.  Using `vec4` (GLSL) / `core::Vec4f` (C++) gives the same 16-byte footprint in both and avoids the mismatch.

### Triplanar normal technique

Used the "whiteout blend" approach (Ben Golus, 2017): for each axis-aligned projection, the decoded tangent-space normal is added to the matching component of the world-space surface normal before normalising.  This keeps all three projected normals leaning toward the true surface, and their weighted average produces a plausible result without a full object-space normal bake.

### TBN construction

The terrain UV mapping is a simple top-down planar projection (world XZ → UV), so the tangent is always aligned with world X projected onto the tangent plane.  A fallback to the world Z axis is included for the degenerate case where the surface normal is nearly parallel to world Y.

### TerrainPatchInfos extension

Two float padding members (`tpi_pad1_`, `tpi_pad2_`) keep the struct at an exact multiple of float4 (80 bytes = 5 × 16), satisfying std140 requirements.  Static assertions validate all offsets.

## Output to keep in mind

- `kPatchInfosFloat4s` in `TerrainRenderer.cpp` is derived from `sizeof(TerrainPatchInfos) / 16`; it automatically tracked the 48 → 80 growth.
- The constant buffer slot (`kPatchInfosSlot = 6`) is unchanged.
- `TerrainRenderer::SetMaterial()` and `SetMacroTexture()` are non-owning; callers must ensure the pointed-to objects outlive the renderer.
- Macro texture scale (`kMacroScale = 0.05`) and triplanar sharpness (`kTriplanarSharpness = 4.0`) are shader constants; consider exposing them via the UBO if tuning is needed at runtime.

## Skills and instructions consulted

- `src/CLAUDE.md` — Google C++ style, include-root rules, one-class-per-file
- `src/terrain/CLAUDE.md` — Y-up coordinate system, dependency rules
- `data/shaders/glsl/CLAUDE.md` — `#include` usage, Blend GLSL coding style, separate VS/PS files
- `data/CLAUDE.md` — folder structure
- `.claude/rules/glsl-shaders.md` — separate VS/PS files rule
