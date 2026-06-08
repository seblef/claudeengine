# Terrain Normalization at Import (#397)

## Overview

Heightmaps imported from PNG, HDR, or EXR files were not normalised before being
mapped to the editor's configured height range. If an external tool exported
elevation data with values spanning e.g. [100, 200] instead of [0, 255], the
resulting terrain would only use the upper portion of the [min, max] height range.

## Changes

**`src/editor/TerrainEditorPanel.cpp`**

- `LoadAndApplyPNG`: added a min/max scan over the raw `uint8_t` pixel array.
  The normalised value `t = (pixel - min_px) / (max_px - min_px)` replaces the
  previous `pixel / 255.f`.

- `LoadAndApplyHDR`: removed the `std::clamp(pixels[i], 0.f, 1.f)` guard (which
  silently discarded out-of-range values) and replaced it with a min/max scan and
  normalisation: `t = (pixels[i] - min_val) / (max_val - min_val)`. This works
  correctly for EXR files with raw world-space elevation values.

- Import window hint text updated to describe the normalisation behaviour.

## Decisions

- **Flat-terrain edge case**: when all pixels are identical (`max == min`), the
  range is set to `1.f` and all t values become `0`, mapping every texel to
  `import_min_h_`. This is the most sensible interpretation of a flat heightmap.

- **No clamp after normalisation**: since normalisation guarantees t ∈ [0, 1] by
  construction, the previous `std::clamp` in the HDR path was removed. Floating
  point precision cannot push t outside [0, 1] because the min/max are computed
  from the same array.

- **No behaviour change for well-formed files**: a PNG that already spans [0, 255]
  or an HDR that already spans [0.0, 1.0] will produce exactly the same result as
  before after normalisation.

## Output to keep in mind

- The min/max scan adds O(n) work at import time, which is negligible for the
  heightmap sizes used in this engine.
- The `LoadAndApplyPNG` / `LoadAndApplyHDR` methods are the only two import paths;
  there is no R16 import (R16 is a raw uint16 format that already encodes the
  full height range as-is).

## Skills and instructions used

- `impl-issue` skill
- `src/CLAUDE.md`: Google C++ style, include root is `src/`, one class per file
- `src/editor/CLAUDE.md`: editor is the leaf of the dependency graph, no game code
- Root `CLAUDE.md`: conventional commit message, history file requirement, cpplint
  before commit, PR to `dev`
