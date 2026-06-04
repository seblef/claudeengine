# Terrain Auto-Paint Helper (Issue #377)

## Overview

Added a floating **Auto-Paint Terrain by Height** window that lets the user
paint the entire terrain splatmap in one click, using height ranges to assign
material layers.

## Changes

### New files

| File | Purpose |
|------|---------|
| `src/editor/TerrainPainterWindow.h` | Declaration of `TerrainPainterWindow` |
| `src/editor/TerrainPainterWindow.cpp` | Implementation — UI + painting algorithm |

### Modified files

| File | Change |
|------|--------|
| `src/editor/TerrainEditorPanel.h` | Added `<memory>` include; forward-declared `TerrainPainterWindow`; declared explicit constructor/destructor (required by `unique_ptr` to incomplete type); added `RenderPainterWindow()` and `painter_window_` member |
| `src/editor/TerrainEditorPanel.cpp` | Includes `TerrainPainterWindow.h`; defines ctor/dtor; forwards context to painter_window_ in `SetContext()`; adds "Auto-Paint by Height…" button in Paint tab; adds `RenderPainterWindow()` passthrough |
| `src/editor/EditorWindow.cpp` | Calls `terrain_panel_.RenderPainterWindow()` each frame alongside `RenderImportWindow()` |
| `src/editor/CMakeLists.txt` | Added `TerrainPainterWindow.cpp` to the editor static library |

## Design decisions

### Window vs. tab
A **floating window** was chosen (like the existing `TerrainImportWindow`) so
the user can keep it open while inspecting the terrain and iterate quickly.
It is opened via the **"Auto-Paint by Height…"** button at the bottom of the
Paint tab.

### Height-range list
The UI presents a resizable table of entries — each row holds a layer index
(int slider 0–N), a `min_height` and a `max_height` (float drag inputs).
Rows can be added or removed dynamically. The number of rows is unbounded but
capped implicitly by the fact that only four splatmap channels exist
(`kMaxTerrainLayers = 4`).

### Painting algorithm
For each splatmap texel `(sx, sz)`:
1. Sample the world-space height `h` via `TerrainData::GetHeight()`.
2. Add a deterministic noise perturbation: `h_noisy = h + Noise(sx, sz) × noise_amplitude`.
3. For each HeightRange entry compute a smoothstep weight that ramps up above
   `min_height` and ramps down below `max_height`; the ramp half-width is
   `blend_half = max(0.5, noise_amplitude × 0.6)`.
4. Accumulate weights per material layer (multiple entries can target the same
   layer and their weights are summed).
5. Normalise so channels sum to 1; fall back to layer 0 when no range covers
   the texel.
6. Write via `TerrainMaterial::SetSplatWeight()`.

### Noise
`Noise(x, z)` is a cheap deterministic hash in `[-1, 1]` based on the integer
texel coordinates. It requires no external library and produces visually
acceptable organic patterns for layer boundaries.

`noise_level ∈ [0, 1]` scales the amplitude to `±15 %` of the full terrain
height range at maximum. The blend-zone half-width is proportional to the
same amplitude, so both the "raggedness" and the "softness" of boundaries
increase together with `noise_level`.

### Undo / redo
The full splatmap is snapshotted before and after painting. If the snapshots
differ, a `PaintBrushCommand(0, 0, sw, sh, pre, post)` is pushed to the
command history so the operation can be undone with `Ctrl+Z` in a single step.

## Output to keep in mind

- `TerrainPainterWindow` is owned by `TerrainEditorPanel` as a lazily-created
  `unique_ptr`; its context is refreshed whenever `SetContext()` is called.
- `RenderPainterWindow()` must be called **outside** any `ImGui::Begin/End`
  block (same constraint as `RenderImportWindow()`); `EditorWindow::Render()`
  already does this.
- The explicit `TerrainEditorPanel::~TerrainEditorPanel() = default` in the
  `.cpp` is load-bearing: without it the compiler cannot instantiate
  `unique_ptr<TerrainPainterWindow>` at the point where the destructor is
  generated (incomplete-type rule).

## Skills and instructions used

- `src/CLAUDE.md` — one class per .h/.cpp, Google C++ style, include root `src/`
- `src/editor/CLAUDE.md` — one class per pair, ImGui calls outside Begin/End,
  no singletons, BUILD_EDITOR=ON requirement
- `src/terrain/CLAUDE.md` — dependency rules (terrain ← editor direction)
- Global CLAUDE.md git workflow (branch from dev, cpplint, conventional commit, PR)
