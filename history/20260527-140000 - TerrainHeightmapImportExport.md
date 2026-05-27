# Terrain Heightmap Import / Export (#297)

## Summary

Extends `TerrainEditorPanel` with an **Import / Export** tab allowing users to
import heightmaps from external tools and export the current heightmap for
external editing.

## Changes

### `terrain/TerrainData` — `ReplaceHeightmap()`
New public method that overwrites the internal `uint16_t` buffer and updates
`width_` / `height_`. Callers are responsible for subsequently rebuilding the
normal map, re-uploading the GPU heightmap, and rebuilding the CDLOD quadtree.

### `terrain/TerrainMaterial` — `ResetSplatmap()`
New public method that reinitialises the splatmap CPU buffer to the default
(all weight on layer 0, RGBA = {255,0,0,0}) for a given size and re-uploads to
GPU. Used after a resize-import to avoid a stale splatmap.

### `terrain/TerrainRenderer` — `lod_count_` + `Rebuild()`
`lod_count_` is now stored from `Init()`. New `Rebuild(data)` method recreates
the heightmap GPU texture and rebuilds the CDLOD quadtree using the stored
`patch_size_` and `lod_count_`. Needed when the imported heightmap dimensions
differ from the current terrain.

### `editor/stb_image_impl.cpp`
New translation unit implementing `stb_image` (via `STB_IMAGE_IMPLEMENTATION`)
so that the editor module can load PNG and HDR heightmaps without polluting other
modules. The existing `stb_image_write_impl.cpp` covers the write side.

### `editor/CMakeLists.txt`
`stb_image_impl.cpp` added to the editor static library.

### `editor/TerrainEditorPanel` — Import / Export tab

#### UI
New **Import / Export** tab with:
- **Import section**: Min/Max Height sliders (for PNG brightness→world-height
  mapping), "Import PNG..." button, "Import HDR / EXR..." button.
- **Export section**: "Export PNG..." button (8-bit grayscale), "Export R16..."
  button (raw `uint16_t` binary).
- Status label showing the last operation result (green = success, red = error).
- Resize-confirmation modal that appears when the imported image dimensions
  differ from the current terrain, warning that the splatmap will be reset.

#### Import PNG
`stbi_load()` with `desired_channels=1` (forced grayscale). Each 8-bit value
maps to `[import_min_h_, import_max_h_]` using the configurable Min/Max Height
params, then converted to `uint16_t` via `TerrainData::HeightToSample()`.

#### Import HDR / EXR
`stbi_loadf()` with `desired_channels=1`. Float values are treated as world-
space heights in metres, clamped to the terrain's `[min_height, max_height]`
range, then converted to `uint16_t`. **Note**: stb_image supports the RGBE
(`.hdr`) format but not true OpenEXR. Files with `.exr` extension are accepted
but will fail to load unless they are in RGBE-encoded HDR format.

#### Export PNG
Current `uint16_t` samples are mapped linearly to `[0, 255]` and written as an
8-bit grayscale PNG via `stbi_write_png()`.

#### Export R16
Raw `uint16_t` buffer (row-major, same layout as the map serialiser) written to
a binary file via `std::fwrite`.

#### After-import pipeline
1. `TerrainData::ReplaceHeightmap()` — update CPU buffer (+ dimensions if resize)
2. `TerrainNormalMap::Build()` + `Upload()` — full normal-map rebuild
3. If **same dimensions**: `TerrainRenderer::UpdateHeightmapTile()` over the
   full extent (avoids texture re-creation).
4. If **different dimensions**: `TerrainRenderer::Rebuild()` (new GPU texture +
   new quadtree) and `TerrainMaterial::ResetSplatmap()`.
5. `EditorCommandHistory::Clear()` — the undo stack is no longer valid after a
   full heightmap replacement.

## Decisions and rationale

- **EXR via stb_image**: True OpenEXR support would require a dedicated library
  (e.g. TinyEXR). The issue explicitly allows documenting the stb limitation, so
  stbi_loadf is used as-is with a UI note. If proper EXR support is needed in
  the future, TinyEXR can be added as an `external/` FetchContent dependency.
- **Synchronous import**: For typical terrain sizes (up to 2048²) stb_image
  loading + HeightToSample conversion + GPU upload is fast enough (< 1 s) that
  a background thread adds complexity with little benefit. If 4096² or larger
  terrains become common, a threaded loader with a progress callback can be added.
- **`const_cast` avoided**: `DoExportPNG` and `DoExportR16` are non-const so
  they can write `io_status_msg_` cleanly.
- **History cleared on import**: Sculpt/paint commands in the undo stack
  reference the pre-import heightmap data, so the stack is invalidated and cleared.

## Notes for next features

- EXR support could be added via TinyEXR without changing the panel API.
- A background-thread import path would need a `std::atomic<float>` progress
  variable and a mutex-protected completion callback.
- The R16 export format matches the `MapSerializer` binary heightmap layout;
  future importers for that format should use the same loading path here.

## Skills and instructions used

- `impl-issue` skill for the overall contribution workflow.
- `src/CLAUDE.md`: one class per file, Google C++ style, `src/` include root.
- `src/editor/CLAUDE.md`: editor is the leaf of the dependency graph; no editor
  headers in terrain/.
- `src/terrain/CLAUDE.md`: terrain must not depend on editor or gldevices.
