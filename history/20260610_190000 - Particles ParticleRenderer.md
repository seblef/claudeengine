# Particles — ParticleRenderer (geometry pass, kGBuffer solid particles)

**Date**: 2026-06-10
**Branch**: feat/particle-renderer-gbuffer
**Issue**: #450
**PR**: closes #450

---

## Summary

Implemented `ParticleRenderer`, which renders solid (`kGBuffer`) particle
emitters as billboard quads into the deferred G-buffer. Solid particles now
receive full deferred lighting without any additional rendering cost.

---

## New files

| File | Purpose |
|---|---|
| `src/particles/ParticleRenderer.h` | Class declaration |
| `src/particles/ParticleRenderer.cpp` | Shared IBO construction, per-emitter draw calls |
| `data/shaders/glsl/geometry/particle_gbuffer_vs.glsl` | Billboard expansion vertex shader |
| `data/shaders/glsl/geometry/particle_gbuffer_ps.glsl` | G-buffer output fragment shader |

---

## Modified files

| File | Change |
|---|---|
| `src/particles/CMakeLists.txt` | Added `ParticleRenderer.cpp`; changed dep from `renderer` to `abstract` |
| `src/renderer/CMakeLists.txt` | Added `particles` as a PRIVATE dep |
| `src/renderer/Renderer.h` | Added `SetParticleRenderer()` and `particle_renderer_` member; forward-declared `particles::ParticleRenderer` |
| `src/renderer/Renderer.cpp` | Added `#include "particles/ParticleRenderer.h"`; call at end of geometry pass |
| `src/particles/CLAUDE.md` | Updated dep graph and module table |

---

## Decisions and rationale

### Dependency graph inversion

The `particles/CLAUDE.md` originally documented the dependency as
`game → particles → renderer`. The issue spec placed `ParticleRenderer` in
`src/particles/`, which meant `renderer` would have to include `particles` —
creating a circular dependency (`particles → renderer → particles`).

**Resolution**: Inverted the relationship to match how `terrain/` is handled:
```
renderer → particles → abstract → core
```
- `particles/CMakeLists.txt` now links `core abstract` (renderer was unused).
- `renderer/CMakeLists.txt` now links `particles` as a PRIVATE dep.
- `particles/CLAUDE.md` updated to document the corrected graph.

### Shared index buffer

Built once at construction with capacity for `65536` particles (6 indices ×
4 vertices per quad, uint32 indices). Each emitter draw uses the prefix
`[0, particle_count*6)` of this IBO. The IBO is reused across all emitters.

### Billboard expansion in the vertex shader

All 4 vertices of a billboard quad carry identical data (`center`, `size`,
`color`, `uv_offset`). The vertex shader uses `gl_VertexID % 4` to pick the
corner offset. Right/up vectors are extracted from rows 0 and 1 of the
(row-major) view matrix so billboards always face the camera.

### World-space normal for camera-facing billboards

The G-buffer expects world-space normals encoded to `[0, 1]`. For a
billboard that always faces the camera the view-space normal is `(0, 0, 1)`.
Converting to world space: `view^T * (0,0,1)` = column 2 of view =
`(view[2][0], view[2][1], view[2][2])` with row-major indexing, negated so
it points toward the viewer.

### Alpha test threshold

Discard if `texture_color.a * color.a < 0.5`. Keeps the billboard opaque
while still allowing sprite-sheet sprites with transparent borders to be
clipped cleanly.

### Depth state

Depth write ON, `LEQUAL` test for the particle geometry pass. Restored to
`LESS` after the pass so subsequent passes are unaffected.

---

## Output / keep in mind

- `ParticleRenderer` is not a singleton — callers (`GameSystem`,
  `EditorSystem`) create it and pass it via `Renderer::SetParticleRenderer()`.
- Next: `kAdditive` and `kAlphaBlend` particles will need a forward pass
  (after the emissive pass). The same `ParticleRenderer` can be extended with
  a `RenderForwardPass()` method.
- The shared IBO capacity constant `kMaxIBOParticles = 65536` is defined in
  an anonymous namespace in `ParticleRenderer.cpp`. Increase if large emitters
  are needed.
- Shader name: `geometry/particle_gbuffer` → the GLSL files live in
  `data/shaders/glsl/geometry/particle_gbuffer_{vs,ps}.glsl`.

---

## Skills used

- `impl-issue`

## CLAUDE.md / spec changes proposed

- `src/particles/CLAUDE.md`: Updated dependency graph from
  `game → particles → renderer` to `renderer → particles → abstract`.
  Also updated the Module structure table to include `ParticleEmitter` and
  `ParticleRenderer` (they were missing).
- Suggest adding to `src/particles/CLAUDE.md` guidelines: "Do not depend on
  `renderer` — `abstract` is sufficient for GPU resource creation."
