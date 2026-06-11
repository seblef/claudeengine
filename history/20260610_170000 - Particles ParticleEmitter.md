# Particles — ParticleEmitter

**Issue**: #449
**PR**: #457
**Branch**: `feat/particle-emitter`
**Date**: 2026-06-10

## Changes

### New files

| File | Role |
|---|---|
| `src/core/VertexParticle.h` | Billboard vertex: `center` (vec3), `size` (float), `color` (vec4), `uv_offset` (vec2) |
| `src/particles/ParticleEmitter.h` | Public API + private `Particle` struct |
| `src/particles/ParticleEmitter.cpp` | Full CPU simulation and GPU upload |

### Modified files

| File | Change |
|---|---|
| `src/core/VertexType.h` | Added `kParticle` to the enum and `sizeof(VertexParticle)` to `kVertexSize` |
| `src/gldevices/GLVertexBuffer.cpp` | Added `kParticle` case in `ConfigureAttributes` (locations 0–3) |
| `src/particles/CMakeLists.txt` | Added `ParticleEmitter.cpp` to the static library |

## Decisions

**`VertexParticle` layout** — The four fields (center vec3, size float, color vec4, uv_offset vec2) give a 48-byte stride. The `Color` member has `alignas(16)`, which forces struct alignment to 16 and pads the struct to 48 bytes. This mirrors the existing `VertexBase` layout. Using `Color` directly (rather than four raw floats) keeps the type system consistent across the codebase.

**All four billboard vertices carry identical data** — The vertex shader distinguishes corners via `gl_VertexID % 4`. This avoids a per-corner delta field in the vertex layout and is consistent with standard GPU billboard techniques. The shared index buffer (mentioned in the issue as "shared across all emitters") is the renderer's responsibility.

**Rodrigues' rotation formula for cone spread** — The canonical local direction is generated around Y=(0,1,0) using Archimedes' hat-box theorem for uniform spherical-cap sampling, then rotated to the actual `desc_.direction` via Rodrigues. Special cases for parallel/anti-parallel directions use identity and Y-flip respectively.

**Per-particle `size_start`/`size_end`** — The issue's `Particle` struct spec only has `size` (current). Since the descriptor provides min/max ranges for both start and end sizes, each particle randomises its own start/end at spawn time and lerps between them. Two extra private fields (`size_start`, `size_end`) were added to the struct; `color_start`/`color_end` are shared across all particles (no range in the descriptor) so they are read directly from `desc_` during interpolation.

**`kDynamic` VBO pre-allocated to `max_particles * 4`** — Matches the issue spec. The VBO is never resized; `Fill()` uploads only the live prefix each frame. The CPU-side `vbo_vertices_` vector is also pre-allocated at construction to avoid per-frame allocation.

**`cppcheck-suppress unusedStructMember`** — Added to all `Particle` struct members and several `ParticleEmitter` private fields. `cppcheck` cannot see the `.cpp` usage from the `.h` analysis pass; this is the established project pattern (see `ParticleSubSystemDesc.h`, `GameLight.h`, etc.).

## Output to keep in mind

- **Index buffer**: the particle renderer must create a shared index buffer for `max_particles * 6` indices (two triangles per quad: 0,1,2, 2,1,3) and reuse it across all emitters. `ParticleEmitter` only owns the VBO.
- **`sprite_cols` / `sprite_rows` uniforms**: must be passed per-draw-call so the shader can compute the per-frame UV size from `uv_offset`.
- **`done_` flag**: one-shot emitters set `done_ = true` when `elapsed_ > duration && live_count_ == 0`. The renderer / `ParticleSystem` manager should query this to remove finished emitters.
- **`SetWorldTransform`**: extracts translation from `transform(row, 3)` (column 3, row-major). Only the spawn origin is updated; existing live particles keep their world-space positions.
- **Gravity sign**: `desc_.gravity > 0` means downward acceleration (−Y). The gravity vec is `{0, -gravity, 0}`.

## Skills / instructions used

- `impl-issue` skill
- `src/CLAUDE.md` — one class per file, include root `src/`, Google C++ style
- `src/particles/CLAUDE.md` — no platform headers, one class per file
- `src/gldevices/GLVertexBuffer.cpp` — pattern for adding a new vertex attribute layout
- `history/20260610_161934 - Particles Core Module.md` — context for sprite animation rules and YAML conventions
- Project `CLAUDE.md` — conventional commits, history file, PR to `dev`, cpplint before commit
- Memory: `feedback_cppcheck_suppressions.md` — `cppcheck-suppress unusedStructMember` on members used only in `.cpp`
