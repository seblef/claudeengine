# Terrain Ridged Multi-fractal Generation

**Issue**: #400
**Branch**: `feat/terrain-ridged-multifractal-400`
**Date**: 2026-06-08

## Summary

Extended the procedural terrain generation system with a second algorithm —
Ridged Multi-fractal — producing sharp mountain ridges and dramatic peaks,
distinct from the smoother fBm output already available.

## Changes

### `src/terrain/TerrainGenerator.h`

- Added `RidgedParams` struct with fields: `seed`, `scale`, `octaves`,
  `lacunarity`, `gain`, `offset` (matching default values from issue spec).
- Added `GenerateRidged` static method declaration to `TerrainGenerator`.
- Updated class-level doc comment to reflect two algorithms instead of one.

### `src/terrain/TerrainGenerator.cpp`

- Implemented `TerrainGenerator::GenerateRidged`:
  - Seed-based domain shifting via existing `kSeedScaleX` / `kSeedScaleZ`
    constants (same strategy as `GenerateFbm` — stb_perlin_ridge_noise3 has no
    seed parameter, so we offset the sampling coordinates).
  - Uses `stb_perlin_ridge_noise3(nx, 0.f, nz, lacunarity, gain, offset, octaves)`
    directly; stb handles the multi-octave accumulation internally.
  - Normalises output range to [0, 65535] before returning — same post-process
    as fBm.

### `src/editor/TerrainEditorPanel.h`

- Added `kRidged` value to `GenAlgorithm` enum.
- Added `gen_ridged_params_` member (`terrain::RidgedParams`), annotated with
  `// cppcheck-suppress unusedStructMember`.

### `src/editor/TerrainEditorPanel.cpp`

- Updated `RenderGenerateWindow()`:
  - Algorithm dropdown now has two entries: `"fBm (Fractal Brownian Motion)"` and
    `"Ridged Multi-fractal"`.
  - fBm parameters block is now shown **only** when `gen_algorithm_ == kFbm`.
  - New Ridged parameters block shown **only** when `gen_algorithm_ == kRidged`,
    with DragFloat/DragInt for all six parameters plus clamped ranges and a
    dedicated "Randomize seed" button.
  - Generate button dispatches to `GenerateFbm` or `GenerateRidged` based on
    `gen_algorithm_`.

## Decisions

- **Seed via domain shift** (not per-octave seeds): `stb_perlin_ridge_noise3`
  does not expose a per-call seed; domain shifting with large prime-derived
  offsets is deterministic and produces visually distinct fields for different
  integer seeds.
- **No new files**: all changes are in the existing `.h`/`.cpp` pairs as
  required by the issue guidelines.
- **Normalisation**: the raw ridge noise output is normalised min→0 / max→65535
  before mapping to `[min_height, max_height]`, matching the fBm pipeline.

## Output to keep in mind

- `RidgedParams` is now available for any future algorithm extensions (e.g.,
  turbulence noise) that might reuse `lacunarity` / `gain` / `offset` fields.
- The domain-shift seed strategy is shared by both algorithms via the anonymous-
  namespace constants `kSeedScaleX` / `kSeedScaleZ` in `TerrainGenerator.cpp`.
- If a third algorithm is added, the `GenAlgorithm` enum and `algo_names[]`
  array in `RenderGenerateWindow` are the only two places to extend.

## Skills / CLAUDE.md instructions used

- `impl-issue` skill
- `src/CLAUDE.md`: one class per `.h/.cpp`, Google C++ style, include root `src/`
- `src/terrain/CLAUDE.md`: no `gldevices`/`editor` headers in terrain module
- `src/editor/CLAUDE.md`: one class per `.h/.cpp`, ImGui calls inside
  `NewFrame`/`Render` brackets
