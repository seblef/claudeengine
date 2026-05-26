# TerrainNormalMap — CPU normal computation with dirty-tile GPU upload

**Issue:** #288  
**Branch:** `feat/terrain-normal-map-288`  
**Date:** 2026-05-26

---

## Summary

Adds `TerrainNormalMap`, a class that derives a world-space normal map from a
`TerrainData` heightmap, uploads it to GPU as an RGBA8 texture, and supports
incremental dirty-tile re-uploads for sculpt-brush performance.

---

## Changes

### New files
| File | Purpose |
|------|---------|
| `src/terrain/TerrainNormalMap.h/.cpp` | CPU computation + GPU upload of the normal map |
| `src/gldevices/GLNormalMapTexture.h/.cpp` | OpenGL RGBA8 texture with `UpdateRegion()` support |

### Modified files
| File | Change |
|------|--------|
| `src/abstract/RawTexture.h` | Added pure-virtual `UpdateRegion(x, y, w, h, data)` |
| `src/abstract/VideoDevice.h` | Added `CreateNormalMapTexture(w, h, data)` factory |
| `src/gldevices/GLRawTexture.h/.cpp` | Implemented `UpdateRegion()` (R16, `GL_RED`/`GL_UNSIGNED_SHORT`) |
| `src/gldevices/GLVideoDevice.h/.cpp` | Implemented `CreateNormalMapTexture()` |
| `src/terrain/CMakeLists.txt` | Added `TerrainNormalMap.cpp` |
| `src/gldevices/CMakeLists.txt` | Added `GLNormalMapTexture.cpp` |

---

## Decisions and rationale

### `RawTexture::UpdateRegion` added as pure virtual

`RawTexture` is the in-engine CPU-owned texture interface (as opposed to
`abstract::Texture` which is file-backed and ref-counted). Adding `UpdateRegion`
there generalises it for any in-engine texture that needs partial updates — both
heightmaps (if sculpting is later added) and normal maps. All existing concrete
classes (`GLRawTexture`) implement it with their respective GL formats.

### Separate `GLNormalMapTexture` class

The heightmap uses R16 (`GL_R16`) while the normal map uses RGBA8 (`GL_RGBA8`).
The upload format and `UpdateRegion` pixel stride differ, so a separate GL
class is cleaner than making `GLRawTexture` format-generic.

### `TerrainNormalMap` stores `const TerrainData*`

`UploadTile` recomputes normals before uploading. Because the specified API
doesn't pass `TerrainData` to `UploadTile`, the class retains a non-owning
pointer set in `Build()`. The caller must ensure `TerrainData` outlives
`TerrainNormalMap` (same lifetime model as `TerrainRenderer`).

### Dirty-tile inflation by 1

Central differences at texel (x, z) sample (x±1, z) and (x, z±1). Recomputing
only the dirty rect would miss edge normals whose inputs lie just outside the
rect. Inflating by 1 on each side (clamped at heightmap borders) ensures
correctness without re-uploading the whole texture.

### Normal encoding

`N * 0.5 + 0.5` per channel as `uint8_t`, matching the issue spec and the
standard octahedral packing convention. Alpha is always 255 (unused, but
RGBA8 is the most universally supported format for this use case).

---

## Output to keep in mind

- `TerrainNormalMap::GetTexture()` returns `abstract::RawTexture*`; bind at
  **slot 1** in the terrain VS (slot 0 is the heightmap).
- `TerrainRenderer::Init()` should be updated in a follow-up to call
  `normal_map_.Build(data)` and `normal_map_.Upload(video)`, and bind the
  texture before draw calls.
- After any sculpt-brush stroke, call `UploadTile` with the AABB of affected
  texels; the class handles inflation internally.

---

## Skills and instructions used

- `impl-issue` skill
- `src/CLAUDE.md`: one class per file, Google C++ style, include root is `src/`
- `src/terrain/CLAUDE.md`: dependency rules (no gldevices in terrain), Y-up world coords
- Root `CLAUDE.md`: Git workflow, history file requirement
