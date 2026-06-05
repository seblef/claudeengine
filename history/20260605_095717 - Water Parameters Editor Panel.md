# Water Parameters Editor Panel (#361)

## Summary

Exposed all `WaterInfos` shader parameters in the environment editor panel with live preview and YAML persistence.

## Changes

### `src/environment/WaterRenderer.h`
- Added new setter methods: `SetWaterColor`, `SetRoughness`, `SetSunIntensity`, `SetRefractionStrength`, `SetAbsorptionScale`, `SetFoamParams`, `SetNormalMapParams`.
- Added corresponding private member fields (defaulting to the same values as `WaterInfos` defaults).
- Added getter methods for all new parameters (used by `Renderer` to fill the `WaterInfos` constant buffer).

### `src/renderer/Renderer.cpp`
- Updated the `WaterInfos` upload to populate all fields from the new `WaterRenderer` getters instead of relying on struct defaults.

### `src/environment/EnvironmentDesc.h`
- Added new water fields: `water_color_r/g/b`, `roughness`, `sun_intensity`, `refraction_strength`, `absorption_scale`, foam parameters, and normal map parameters. All default to the same values as `WaterInfos`.

### `src/game/MapLoader.cpp`
- Extended `ParseEnvironmentDesc` to read all new YAML water fields (fully backward-compatible: missing keys fall through to struct defaults).

### `src/editor/MapSerializer.cpp`
- Extended `EmitEnvironment` to serialize all new fields (only written when non-default to keep YAML concise).

### `src/editor/EnvironmentEditorPanel.cpp`
- `EnableWater`: pushes all new parameters from `EnvironmentDesc` to `WaterRenderer` on enable.
- `RenderWaterSection`: added ImGui controls for all new parameters with live preview via immediate setter calls.

## Design decisions

- **Setters in WaterRenderer rather than direct struct mutation**: `WaterRenderer` owns the values; `Renderer` reads them into `WaterInfos` each frame. This keeps a single source of truth and avoids races where an old CB upload would see stale defaults.
- **Defaults matching WaterInfos struct**: ensures zero-regression when loading old maps without the new YAML keys.
- **SetFoamParams / SetNormalMapParams grouped setters**: the four foam fields and four normal map fields are always set together, reducing per-parameter setter noise in the editor code.
- **MapSerializer emits only non-default values**: keeps the YAML diff small for maps that do not customise water.

## Skills / CLAUDE.md notes used

- `src/CLAUDE.md`, `src/environment/CLAUDE.md`, `src/editor/CLAUDE.md`, `src/game/CLAUDE.md`
- Followed Google C++ style guide, one class per file, no platform headers.
- Live preview pattern established by sky/cloud/wind sections.

## Output to keep in mind

- `WaterRenderer` now exposes full parameter coverage; future water features (e.g. caustic intensity, SSR strength) should follow the same setter/getter pattern and add fields to `EnvironmentDesc`.
- `MapLoader` parse order within `ParseEnvironmentDesc` is not significant; new fields can be appended freely.
