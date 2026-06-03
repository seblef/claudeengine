# Skip terrain binary writes on save when unmodified — #374

## Context

Map saves with terrain were always slow because `MapSerializer::EmitTerrain`
unconditionally wrote three categories of binary side-car files on every save:

| File | Write cost |
|------|-----------|
| `.r16` raw heightmap (uint16, row-major) | Fast but unnecessary I/O |
| Splatmap PNG (`stbi_write_png` encode + write) | CPU-heavy encode |
| Foliage `.r8` density maps | Proportional to map count |

The issue asked that these files only be written when the underlying data has
actually changed.

## Approach: save-dirty flags on data classes

Three dirty flags were introduced, one per binary asset type:

| Class | Flag | Set by | Controls |
|-------|------|--------|----------|
| `TerrainData` | `mutable bool dirty_` | `SetSample()`, `ReplaceHeightmap()` | `.r16` write |
| `TerrainMaterial` | `mutable bool dirty_` | `SetSplatWeight()`, `ResetSplatmap()` | splatmap PNG write |
| `FoliageLayer` | `mutable bool save_dirty_` | `BrushImpl()` | `.r8` density write |

### Why `mutable`?

`MapSerializer::EmitTerrain` receives `const game::GameTerrain*` (and
`GameTerrain::GetFoliageLayer` returns `const FoliageLayer*`). Clearing the
dirty flag after a successful write must be possible in that const context.
Using `mutable` is the standard C++ idiom for observable-state properties
(like mutexes, caches, and I/O coherency flags) that do not represent logical
value changes.

### Why not a single flag on `GameTerrain`?

Three separate flags give finer granularity: sculpting the heightmap does not
mark the splatmap dirty, and painting the splatmap does not mark the heightmap
dirty. This matches the actual independence of the on-disk files.

### Why is `save_dirty_` separate from `FoliageLayer::dirty_`?

`FoliageLayer::dirty_` is a *render* flag — it means "GPU scatter instances
need a rebuild". It starts `true` after construction and is cleared by
`RebuildInstances()`. Using it for saving would force every freshly-loaded
terrain to write all foliage density maps on the first save. The new
`save_dirty_` flag starts `false` and is only set when the user actually
paints or erases foliage.

### Why are `SetMinHeight` / `SetMaxHeight` excluded?

These change how uint16 samples are *interpreted* but do not alter the raw
samples stored in the `.r16` file. The height range is persisted in the
YAML block, which is always rewritten (it is cheap text I/O). Marking the
heightmap dirty for a range-only change would be incorrect.

Similarly, `TerrainMaterial::SetLayerAlbedo/Normal/Tiling` and `AddLayer`
only affect YAML, not the splatmap pixel buffer, so they do not set
`TerrainMaterial::dirty_`.

## Files changed

- `src/terrain/TerrainData.h` / `.cpp` — add `dirty_`, `IsDirty()`, `ClearDirty() const`; mark in `SetSample()` and `ReplaceHeightmap()`
- `src/terrain/TerrainMaterial.h` / `.cpp` — same pattern; mark in `SetSplatWeight()` and `ResetSplatmap()`
- `src/terrain/FoliageLayer.h` / `.cpp` — add `save_dirty_`, `IsSaveDirty()`, `ClearSaveDirty() const`; mark in `BrushImpl()`
- `src/editor/MapSerializer.cpp` — guard each file write behind the corresponding dirty flag; clear dirty after successful write; log skipped writes at `INFO` level

## Skills / instructions used

- `impl-issue` skill
- `src/CLAUDE.md` — one class per file, Google C++ style, include root `src/`
- `src/terrain/CLAUDE.md` — no editor/renderer includes from terrain module
- `src/editor/CLAUDE.md` — editor is leaf of dependency graph
- `CLAUDE.md` root — conventional commits, cpplint, PR to `dev`

## Notes for future contributions

- If a new mutation method is added to `TerrainData` or `TerrainMaterial` that
  changes the on-disk binary content, remember to set `dirty_ = true` in it.
- If a second foliage brush type is added, call it through `BrushImpl()` (or
  set `save_dirty_ = true` explicitly) to ensure the density map is saved.
- Foliage density `.r8` files are the smallest of the three; the PNG splatmap
  encode is the most expensive. Future work could profile exactly how much time
  each write saves on a large terrain.
