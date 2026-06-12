# fix(terrain): move normal layer slots 6–9 to 12–15

**Date:** 2026-06-11
**Branch:** fix/terrain-normal-slots-overlap-481
**PR:** #488 — closes #481

## Problem

`TerrainRenderer` bound normal layers at `kNormalBaseSlot = 6`, occupying sampler slots 6, 7, 8, 9. The lighting pass binds the GBuffer for reading at the overlapping slots 6 (normal RT), 7 (specular RT), and 8 (depth RT). No wrong-data bug existed because the geometry pass always precedes the lighting pass, but the implicit dependency made the code fragile:

- A second-camera or editor-preview render ordering terrain after lighting would see GBuffer textures in its normal sampler slots.
- Any pass reading slots 6–9 without rebinding after the lighting pass sees stale GBuffer content.

## Fix applied

**Option 1 from the issue** — shift normal layers to a non-overlapping range.

`kNormalBaseSlot` changed from `6` to `12`; normal layers now occupy slots **12–15**.

### Final slot layout (no overlaps)

| Slot | Terrain (geometry pass) | Lighting pass |
|------|-------------------------|---------------|
| 0    | Heightmap               | —             |
| 1    | Splatmap                | —             |
| 2–5  | Layer albedos           | GBuffer albedo (5) |
| 6    | —                       | GBuffer normal RT  |
| 7    | —                       | GBuffer specular RT|
| 8    | —                       | GBuffer depth RT   |
| 10   | Macro texture           | —             |
| 11   | Caustic texture         | —             |
| 12–15| Layer normals           | —             |

## Files changed

- `src/terrain/TerrainRenderer.cpp` — `kNormalBaseSlot` 6 → 12
- `data/shaders/glsl/terrain/terrain_ps.glsl` — `layout(binding = …)` for `u_normal0`–`u_normal3` updated to 12–15; slot-layout comment updated

## Decisions

- Chose option 1 (relocate normals) over option 2 (explicit unbind after each pass) because relocation eliminates the overlap structurally rather than relying on correct unbinding. The unbinding already existed via `UnbindMaterialTextures()`; the slot collision was the deeper issue.
- Slots 6–9 in the terrain are now intentionally unused — this gap is fine and documented in the shader comment.

## Skills / instructions used

- `CLAUDE.md` git workflow (checkout dev, pull, branch, cpplint, conventional commit, PR to dev)
- `src/CLAUDE.md` — Google C++ style, one class per file, include root `src/`
- `data/shaders/glsl/CLAUDE.md` — GLSL binding layout conventions

## Output to keep in mind

- Terrain sampler slots 6–9 are now free; any new terrain feature that needs a sampler slot may safely use 6, 7, 8, or 9.
- If a DirectX 12 backend is added later, the same register numbers must be shifted in the HLSL counterpart of `terrain_ps.glsl`.
