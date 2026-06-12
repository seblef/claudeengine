# Specular Float Value in Materials

## Summary

Added a per-material specular intensity multiplier that scales the specular texture sample before it is written to the G-buffer. Default value is `1.0` (no change to existing materials).

## Changes

### `src/renderer/MaterialInfos.h`
- Added `float specular` at std140 offset 52, just after `shininess`.
- The struct stays 64 bytes; added `static_assert(offsetof(MaterialInfos, specular) == 52)`.

### `data/shaders/glsl/uniforms/material_infos.glsl`
- Added `float specular` to the `MaterialInfosBlock` UBO, mirroring the C++ struct.

### `data/shaders/glsl/geometry/gbuffer_ps.glsl`
- Specular output is now `texture(u_specular, v_uv).r * specular` instead of the raw texture sample.

### `src/renderer/MaterialDesc.h`
- Added `SetSpecular(float)` / `GetSpecular()` with default `1.f`.

### `src/renderer/Material.h` / `Material.cpp`
- Added `specular_` member (default `1.f`), getter `GetSpecular()`, setter `SetSpecular(float)`.
- `Set()` fills `mi.specular` when uploading the constant buffer.
- `LoadFromYaml()` reads the optional `specular` YAML key (new format only; old `colors:` sub-node left unchanged for backward compat).
- `Material(const MaterialDesc&, …)` copies `specular_` from the descriptor.

### `src/editor/commands/MaterialPropertyCommand.h` / `.cpp`
- Added `float specular = 1.f` to `MaterialSnapshot`.
- `CaptureSnapshot` / `ApplySnapshot` / `operator==` all include `specular`.

### `src/editor/MaterialEditorWindow.cpp`
- Added `ImGui::SliderFloat("Specular", …, 0.f, 1.f)` widget in `RenderRenderingSection()`, after the Shininess slider.
- `Save()` emits the `specular` key to the YAML file.

## Decisions

- **Range `[0, 1]`**: The specular texture already encodes intensity in `[0,1]`, so the multiplier is clamped to the same range. Values above 1 would overbrighten; if future need arises the range can be widened.
- **Offset placement**: `specular` was inserted at offset 52 (after `shininess`) to keep the struct 64 bytes and avoid changing existing field offsets.
- **No old-format backward compat for `specular`**: The `colors:` sub-node was deprecated; new key goes to the flat section only. Missing key falls back to `1.f`.

## Skills / CLAUDE.md instructions used

- `src/CLAUDE.md`: one class per file, Google C++ style, include root is `src/`.
- `data/shaders/glsl/CLAUDE.md`: `#include` for shared uniforms, document equations in comments.
- `src/editor/CLAUDE.md`: editor is the leaf of the dependency graph; all ImGui calls within `NewFrame`/`Render`.

## Notes for next features

- The terrain and foliage shaders hard-code their specular output and do not include `material_infos.glsl`; they are unaffected by this change.
- If a specular multiplier > 1 is ever needed, widen the `SliderFloat` range and update the YAML doc.
