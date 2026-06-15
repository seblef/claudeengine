# feat(environment): Water normal map texture file support (#530)

## Summary

Replaced the procedural water normal map generation with file-based loading, supporting distinct textures for both normal map layers. Added full editor integration (texture pickers) and YAML persistence.

## Changes

### `src/environment/EnvironmentDesc.h`
- Added `normal_map_texture1` and `normal_map_texture2` string fields. Empty = flat fallback.

### `src/environment/WaterRenderer.h` / `.cpp`
- **Removed** `BuildNormalMap()` and the `NormalMapHeight()` helper (procedural generation).
- **Removed** `kNormalMapSize` constant.
- Added `normal_map_tex2_` (`unique_ptr<RawTexture>`) for the second normal map layer, bound at sampler slot 5 (`kNormalMap2Slot = 5`).
- Added `LoadNormalMap(path)`: loads from file via `LoadRGBA8File()` + `CreateTileableTexture()`. Falls back to a 1×1 flat normal (R=128, G=128, B=255) when the path is empty or the file cannot be decoded.
- Added `SetNormalMapTextures(path1, path2)`: hot-swaps both textures without rebuilding the renderer. Called by the editor panel on load and on picker change.
- `Build()` now seeds both textures with the flat fallback via `LoadNormalMap("")`.
- `Render()` binds `normal_map_tex2_` at slot 5 and unbinds it after the draw.
- `Reset()` resets both normal map textures.

### `data/shaders/glsl/water/water_ps.glsl`
- Added `layout(binding = 5) uniform sampler2D u_normal_map2` declaration.
- Changed `n2` to sample from `u_normal_map2` instead of `u_normal_map`, allowing the two layers to use independent textures.

### `src/game/MapLoader.cpp`
- `ParseEnvironmentDesc()` now reads `normal_map_texture1` and `normal_map_texture2` YAML keys.

### `src/editor/MapSerializer.cpp`
- `EmitEnvironment()` emits `normal_map_texture1` and `normal_map_texture2` when non-empty.

### `src/editor/EnvironmentEditorPanel.cpp`
- `EnableWater()` calls `wr.SetNormalMapTextures()` with the paths from the descriptor.
- `RenderWaterSection()` adds two texture pickers ("Normal map 1" / "Normal map 2") with Pick and Clear buttons, following the moon/night-sky texture pattern.

## Decisions

- **Flat fallback over procedural**: the 1×1 [128,128,255,255] texel is the minimal valid normal map (pointing straight up). It causes zero visible perturbation until the user assigns a file, which is safe. The prior procedural 512×512 is now removed per the issue requirement.
- **`RawTexture` for both layers**: keeps the type consistent with `foam_tex_` and `caustic_tex_`; avoids mixing ref-counted `Texture*` and `unique_ptr<RawTexture>`. File loading uses `LoadRGBA8File()` + `CreateTileableTexture()` (GL_REPEAT, mip-mapped) for correct tileable water appearance.
- **Sampler slot 5 for the second layer**: slots 0–4 were already allocated; 5 is the next free slot and documented in both the shader and the renderer header.
- **`SetNormalMapTextures` is a hot-swap**: calling it after `Build()` is safe — it simply replaces the two `unique_ptr`s and takes effect on the next rendered frame.

## What to keep in mind for next features

- The `kNormalMap2Slot = 5` constant is defined in `WaterRenderer.h` (private section). If future features need more samplers in the water shader, start from slot 6.
- No actual normal map texture assets are shipped with this PR; maps will use the flat fallback until the editor user picks files.
- The `LoadNormalMap()` helper and `SetNormalMapTextures()` pattern can be reused if foam textures ever need file-based loading.

## Skills / CLAUDE.md guidance applied

- `src/CLAUDE.md`: one class per `.h`/`.cpp` pair, include paths project-relative.
- `environment/CLAUDE.md`: `environment` must not depend on `renderer`; dependency is one-way. `EnvironmentDesc` is a plain struct with no methods.
- `editor/CLAUDE.md`: panel classes are pure UI — `RenderWaterSection()` calls setters only; the texture-loading logic lives in `WaterRenderer::LoadNormalMap()`.
- Conventional commit prefix `feat(environment)`.
