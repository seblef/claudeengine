# Hydraulic Erosion Post-Pass (issue #401)

## What changed

### `src/terrain/TerrainGenerator.h`
- Added `ErosionParams` struct (iterations, seed, inertia, capacity, deposition,
  erosion, evaporation, max_steps, min_slope, gravity).
- Added static method `TerrainGenerator::Erode(buf, width, height,
  meters_per_texel, params)`.

### `src/terrain/TerrainGenerator.cpp`
- Added `#include <random>` and `#include <loguru.hpp>`.
- Implemented `Erode`: particle-based hydraulic erosion (Sebastian Lague / Łacki
  style). Converts uint16_t→float [0,1] at entry, simulates N water droplets
  (each follows the gradient with inertia, picks up sediment when moving downhill,
  deposits when uphill or over capacity), then clamps and writes back to uint16_t.
- Emits `LOG_F(WARNING, ...)` when texel count exceeds 1 024 × 1 024.

### `src/editor/TerrainEditorPanel.h`
- Added three members to the generate-window state block:
  `gen_erosion_enabled_`, `gen_erosion_params_` (with cppcheck suppression).

### `src/editor/TerrainEditorPanel.cpp`
- Added an **Erosion** `CollapsingHeader` section between the algorithm params
  and the Generate button: enable checkbox + sliders for iterations, seed,
  inertia, capacity, deposition, erosion, evaporation.
- The `Erode()` call is inserted after generation and before
  apply/dispatch — only when `gen_erosion_enabled_` is true.

## Decisions & rationale

| Decision | Rationale |
|---|---|
| Static method on existing class, no new file | Issue spec; keeps the terrain module surface minimal |
| Float-space arithmetic, uint16 only at boundaries | Prevents quantisation drift over many droplet steps |
| `std::mt19937` seeded from `ErosionParams::seed` | Deterministic, not dependent on stb_perlin seed path |
| Loguru WARNING at >1 M texels | Issue requirement; INFO would be silenced in stable builds |
| `meters_per_texel` parameter kept (unused, marked `/*…*/`)  | Future slope-aware erosion may weight by real-world scale |
| `ImGui::CollapsingHeader` (closed by default) | Keeps the window compact for users who don't need erosion |

## Output to keep in mind

- Erosion is purely in-memory; no GPU or file I/O involved.
- Parameters are stored per-session on the `TerrainEditorPanel`; not serialised
  to YAML (same as generation params — intentional).
- The Generate window intentionally stays open after generation so the user can
  iterate params freely (existing behaviour, unchanged).

## Skills / instructions followed

- `src/CLAUDE.md`: one class per file, Google C++ style, include root `src/`.
- `src/terrain/CLAUDE.md`: no `gldevices`/`editor` headers in terrain module;
  uint16_t row-major heightmap convention.
- `src/editor/CLAUDE.md`: all ImGui calls inside Begin/End; editor is the
  dependency leaf.
- Project `CLAUDE.md`: history markdown, conventional commit, PR to `dev`.
