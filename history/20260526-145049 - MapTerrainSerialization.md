# Map Terrain Serialization — Issue #293

## Summary

Extended `MapSerializer` and `MapLoader` to persist and restore `GameTerrain`
objects. Terrain data is stored as a root-level `terrain:` key in `.map.yaml`,
with two binary side-car files written alongside or in the data directory.

## Changes

### `src/terrain/TerrainMaterial.h`
Added two read-only accessors needed by the serializer:
- `GetSplatmapPath()` — returns the relative splatmap path (relative to `data/textures/`)
- `GetSplatmapPixels()` — returns the raw RGBA8 CPU buffer for PNG export

### `src/editor/stb_image_write_impl.cpp` (new)
Single translation unit providing the `STB_IMAGE_WRITE_IMPLEMENTATION` define
so `stbi_write_png` is available in the editor module without polluting other TUs.

### `src/editor/CMakeLists.txt`
- Added `stb_image_write_impl.cpp` to the `editor` library.
- Added `PRIVATE ${stb_SOURCE_DIR}` to `target_include_directories` so the
  `<stb_image_write.h>` header is found at compile time.

### `src/editor/MapSerializer.h`
- `SerializeVisitor` constructor now takes `map_path` in addition to `data_dir`
  so it can derive the heightmap/splatmap file names from the map stem.
- Added `EmitTerrain(game::GameTerrain*)` public method that writes the binary
  side-car files and emits the root-level `terrain:` YAML block.

### `src/editor/MapSerializer.cpp`
**Serialization path (`Save`)**
- `SerializeVisitor` is now constructed with `path` (the map file path).
- The objects loop skips `kTerrain` objects (they must not appear in the
  `objects:` sequence); it captures the `GameTerrain*` pointer instead.
- After `out << YAML::EndSeq`, `visitor.EmitTerrain(terrain_obj)` is called to
  write the root-level block while the emitter is back at the top-level map.

**`EmitTerrain` implementation**
1. Derives the map stem from `path.stem().stem()` (strips both `.map` and `.yaml`).
2. Writes `<stem>.r16` next to the `.map.yaml` file — raw little-endian
   `uint16_t` values, row-major, `width × height × 2` bytes.
3. Writes the splatmap PNG to `data/textures/<splatmap_path>` via
   `stbi_write_png` (4 channels, RGBA8).  If `splatmap_path_` is empty a
   default name `<stem>_splat.png` is used.
4. Emits the YAML `terrain:` block with all heightmap parameters and a nested
   `material:` sub-map (layers + splatmap path).

### `src/game/MapLoader.cpp`
**New `ParseTerrain` helper**
- Reads `heightmap_width`, `heightmap_height`, `meters_per_texel`,
  `min_height`, `max_height` from the YAML node.
- Reads the `.r16` file from the map's parent directory into a `vector<uint16_t>`.
- Validates that the file contains exactly `width × height × 2` bytes.
- Constructs `TerrainData` and `TerrainMaterial` (calling `Load()` on the
  nested `material:` sub-node, so existing splatmap loading is reused).
- Returns a `GameTerrain` named `"terrain"`.

**`MapLoader::Load` update**
- After parsing `global_light`, checks for the `terrain:` key.
- Calls `ParseTerrain`; on success, prepends the `GameTerrain` to
  `result.objects` (caller adds it to the scene via `AddDynamicObject`).
- Fully backwards-compatible: maps without a `terrain:` key are unchanged.

## Decisions

- **Terrain at root level, not in `objects:` sequence** — this matches the
  natural mental model (one terrain per map, not just another scene object),
  keeps the objects sequence homogeneous, and makes the YAML readable.
- **`Visit(GameTerrain&)` is a no-op in `SerializeVisitor`** — the visitor
  interface is satisfied; terrain serialization is handled by `EmitTerrain`
  which must be called at root-map level after the objects sequence is closed.
- **`stb_image_write_impl.cpp` in `editor` only** — the write functionality is
  only needed by the serializer; keeping the impl in a dedicated TU avoids
  polluting `MapSerializer.cpp` with the full header implementation.
- **Splatmap path fallback** — if `TerrainMaterial` was constructed with a
  default (no-file) splatmap, `GetSplatmapPath()` is empty; the serializer
  derives a stable `<stem>_splat.png` name from the map stem.

## Output / Things to Remember

- The `.r16` file is always next to the `.map.yaml`. `MapLoader::ParseTerrain`
  therefore uses `map_path.parent_path()` to locate it.
- The splatmap PNG lives in `data/textures/`, consistent with how
  `TerrainMaterial::Load` looks it up.
- `MapSerializer::Load` (the reconstruction path) requires no changes: terrain
  comes out of `MapLoader` as a plain `GameTerrain*` in `map_data.objects` and
  is transferred to the scene via the existing `AddDynamicObject` loop.

## Skills / Instructions Used

- `src/CLAUDE.md` — one class per .h/.cpp, Google C++ style, include root `src/`
- `src/editor/CLAUDE.md` — leaf module, never imported by game/renderer/core
- `src/game/CLAUDE.md` — MapLoader contract: objects NOT added to GameSystem
- `src/terrain/CLAUDE.md` — dependency rules: terrain may not depend on editor
