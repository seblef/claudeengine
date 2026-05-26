# TerrainMaterial — 4-layer splatmap with YAML serialisation

**Date**: 2026-05-26  
**Branch**: feat/terrain-material-splatmap-290  
**Issue**: #290  

---

## What was implemented

### New files

| File | Purpose |
|------|---------|
| `src/terrain/TerrainMaterialLayer.h` | Plain struct (header-only): `albedo_path`, `normal_path`, `tiling` |
| `src/terrain/TerrainMaterial.h` | Class declaration: 4-layer splatmap, YAML Load/Save, CPU/GPU sync |
| `src/terrain/TerrainMaterial.cpp` | Implementation |

### Modified files

| File | Change |
|------|--------|
| `src/terrain/CMakeLists.txt` | Added `TerrainMaterial.cpp` to the `terrain` static library |
| `src/terrain/CLAUDE.md` | Updated class table |
| `src/abstract/VideoDevice.h` | Added `LoadRGBA8File()` virtual method + `<filesystem>`, `<vector>` includes |
| `src/gldevices/GLVideoDevice.h` | Added `LoadRGBA8File()` override declaration |
| `src/gldevices/GLVideoDevice.cpp` | Implemented `LoadRGBA8File()` via `stb_image` |

---

## Key decisions

### `LoadRGBA8File` on VideoDevice

Loading the splatmap PNG into CPU memory requires an image library. The terrain
module is restricted to `core` and `abstract` (no `gldevices`, no platform
headers). Moving file loading into a virtual method on `VideoDevice` keeps stb
strictly inside `gldevices` while giving `TerrainMaterial::Load` a clean,
platform-agnostic call. This is consistent with how `CreateTexture` already does
file loading for asset textures.

### Reuse `CreateNormalMapTexture` for the splatmap GPU texture

The splatmap is RGBA8, identical in format to the terrain normal map. Adding a
semantically-distinct `CreateSplatmapTexture` would duplicate GLNormalMapTexture
with no implementation difference. Since the GPU texture is created once and
updated incrementally via `RawTexture::UpdateRegion`, reusing the existing factory
is the right call.

### `Load` takes `width` and `height`

The issue's signature `Load(YAML::Node, VideoDevice*)` omits dimensions. When no
splatmap file exists on disk we must initialise a default RGBA8 buffer of terrain
size. The dimensions are passed explicitly to avoid a hidden dependency on
`TerrainData` from within the Load call.

### Default splatmap

When no splatmap PNG is found, R=255, G=B=A=0 for every texel — full weight on
layer 0, zero on all others. This matches the issue spec and produces sensible
terrain visuals before any paint work.

### `Save` writes YAML metadata only

`Save(YAML::Emitter&)` serialises layer paths, tiling, and the splatmap file path.
Writing the splatmap PNG to disk is a separate editor responsibility (would use
stb_image_write, not yet added).

---

## What to keep in mind for next features

- **Splatmap PNG write**: `SaveSplatmapPNG(const std::filesystem::path&)` — would
  use `stb_image_write` from gldevices. Could live on VideoDevice as
  `WriteRGBA8File()` following the same pattern as `LoadRGBA8File`.
- **Shader binding**: `TerrainMaterial::Bind(slot_start)` will need to bind the 4
  albedo textures, 4 normal textures, and the splatmap texture to consecutive
  sampler slots for the terrain fragment shader.
- **Splatmap brush**: An editor tool will call `SetSplatWeight` + `UploadSplatTile`
  per paint stroke. The tile granularity should match the brush footprint.
- **Layer count in constant buffer**: The shader needs to know `layer_count_` at
  runtime. Consider adding it to `TerrainPatchInfos` or a separate terrain
  material constant buffer.

---

## Skills and instructions followed

- `src/CLAUDE.md`: one class per `.h/.cpp`, Google C++ style, include root `src/`
- `src/terrain/CLAUDE.md`: dependency rules (abstract + core only, no gldevices
  in terrain), world-space Y-up convention, dirty-tile upload pattern
- CLAUDE.md: conventional commit, history file, PR to `dev`, branch from `dev`
