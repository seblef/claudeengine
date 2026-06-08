# Terrain fBm Procedural Generation (Issue #399)

## Summary

Added fBm (Fractal Brownian Motion) based procedural terrain generation to the editor.

## Changes

### New files

- **`src/terrain/TerrainGenerator.h`** — Declares `terrain::FbmParams` struct and `terrain::TerrainGenerator` class with a single static method `GenerateFbm`.
- **`src/terrain/TerrainGenerator.cpp`** — Implements `GenerateFbm`. Uses `stb_perlin_noise3_seed` per octave with a per-octave seed offset to build the fBm sum, plus a large prime-based domain shift derived from the integer seed so distinct seeds produce visibly different fields even though stb truncates the seed to uint8.
- **`src/terrain/stb_perlin_impl.cpp`** — Single translation unit that defines `STB_PERLIN_IMPLEMENTATION` before including `stb_perlin.h`, following the project pattern for stb single-header libraries.

### Modified files

- **`src/terrain/CMakeLists.txt`** — Added `stb_perlin_impl.cpp`, `TerrainGenerator.cpp`, and the `${stb_SOURCE_DIR}` include path to the terrain static library.
- **`src/editor/TerrainEditorPanel.h`** — Added `#include "terrain/TerrainGenerator.h"`, two new public methods (`OpenGenerateWindow`, `RenderGenerateWindow`), and private state members (`show_generate_window_`, `gen_algorithm_`, `gen_world_width_`, `gen_world_depth_`, `gen_meters_per_texel_`, `gen_min_h_`, `gen_max_h_`, `gen_fbm_params_`).
- **`src/editor/TerrainEditorPanel.cpp`** — Implemented `OpenGenerateWindow` and `RenderGenerateWindow`. The generate window feeds into the existing `ApplyImportedHeightmap` / `on_create_from_import_` / `io_state_ = IoState::kConfirmResize` pipeline, reusing the resize-confirmation modal.
- **`src/editor/EditorWindow.cpp`** — Added `terrain_panel_.RenderGenerateWindow()` call in the render loop and a `Generate...` menu item under the `Terrain` menu.

## Design decisions

### Seeding strategy
`stb_perlin_fbm_noise3` does not accept a seed. `stb_perlin_noise3_seed` does, but the seed is truncated to `uint8_t` (256 distinct permutation tables). To make different integer seeds produce visibly different noise fields, a large prime-based domain shift (`seed * 127.1f`, `seed * 311.7f`) is added to the x/z noise coordinates before sampling. Each octave additionally uses a unique seed (`(seed + o * 73) & 0xFF`) to prevent correlated octaves.

### Normalisation
The raw fBm sum is min/max-normalised to fill `[0, 65535]` across the generated map. This ensures the full height range is used regardless of octave count and parameters.

### Reuse of the import pipeline
The generate window routes its output through the same `ApplyImportedHeightmap` function and `IoState::kConfirmResize` modal as the file import path. No new state machines were needed.

### No algorithm tab, just a dropdown
The issue specifies a dropdown for the algorithm selector, which leaves room for future algorithms without changing the panel structure.

## Skills and instructions used

- `src/CLAUDE.md` — one class per `.h/.cpp`, include root is `src/`, Google C++ style
- `src/terrain/CLAUDE.md` — dependency rules (no editor/gldevices), heightmap format, conventions
- `src/editor/CLAUDE.md` — editor module is the dependency graph leaf, all ImGui calls must be bracketed

## Output to keep in mind

- `stb_perlin.h` is the `stb_SOURCE_DIR` FetchContent download; it must be added to `target_include_directories` for any module that wants to use it outside `gldevices`/`editor`.
- The `stb_perlin_impl.cpp` pattern (one TU per stb header that has implementation code) is now established for the `terrain` module.
- `on_create_from_import_` is the callback invoked when no terrain exists yet; the generate window reuses it so creating a terrain from procedural generation follows the same flow as creating from a file import.
