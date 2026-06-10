# Textured night sky

**Date**: 2026-06-10
**Branch**: feat/textured-night-sky
**Issue**: #443

## Summary

Replaces the procedural star-noise night sky with an optional equirectangular
texture, while keeping the existing procedural implementation as the fallback.

## Changes

### Data structures

- **`EnvironmentDesc`** — added `night_sky_texture` (string, path relative to
  `data/textures/`, empty = procedural fallback).
- **`SkyInfos`** — replaced the unused `si_pad2_` padding float with
  `has_night_sky_texture` (1.0 when the texture is bound). No change to struct
  size or std140 layout (still 32 bytes / 2 float4s).
- **`sky_infos.glsl`** — mirrored the struct change (`si_pad2_` →
  `has_night_sky_texture`).

### Renderer (`SkyRenderer`)

- Added `night_sky_tex_` member (raw pointer owned/released like `moon_tex_`).
- Added `SetNightSkyTexture(path)` method: loads (or clears) the texture from
  the path; no-op before `Build()`.
- `Render()`: fills `has_night_sky_texture`, binds the texture at sampler slot 1
  when present, unbinds after drawing.
- `Reset()`: releases `night_sky_tex_` before `moon_tex_`.

### Shader (`sky_ps.glsl`)

Added `layout(binding = 1) uniform sampler2D night_sky_tex`.

Night sky section is now split into two branches controlled by
`has_night_sky_texture`:

1. **Textured path** — samples the equirectangular texture using spherical
   (lon, lat) coordinates mapped to UV `[0,1]²`, applies a blue-tinted horizon
   gradient, scales by `night_factor`.
2. **Procedural fallback** — unchanged star-noise and dark-blue gradient.

### Persistence

- **`MapLoader.cpp`** — reads `night_sky_texture` from map YAML.
- **`MapSerializer.cpp`** — writes `night_sky_texture` when non-empty.

### App

- **`main.cpp`** — calls `SetTurbidity`, `SetMoonTexture`, and
  `SetNightSkyTexture` from `env` after building `SkyRenderer`, matching the
  editor panel setup that was already complete.

### Editor (`EnvironmentEditorPanel`)

- `EnableSky()` now also calls `SetNightSkyTexture(desc.night_sky_texture)`.
- `RenderSkySection()` — added "Pick night sky texture" / "Clear night sky
  texture" buttons, mirroring the existing moon texture controls.

## Design decisions

- **Equirectangular projection** was chosen because it is the de-facto standard
  for spherical sky/HDRI textures and trivially invertible from a view direction.
- **Slot 1** for the night sky sampler does not conflict with the moon texture
  (slot 0) or any other sampler in the sky shader.
- **Reuse `si_pad2_`** rather than growing the constant buffer, keeping it at
  32 bytes and avoiding a reallocation path.
- **Day blending**: the texture is multiplied by `night_factor` (which is already
  0 at full daylight), so no extra blending logic was needed.
- **Horizon tint**: a blue-black gradient is applied near the horizon to produce
  a smooth transition into the Preetham sky at dusk/dawn.

## Notes for future features

- The night sky texture is currently static (does not rotate with time). If
  star-field rotation is desired, a rotation matrix derived from `time_of_day`
  could be applied to the view direction before projecting to UV.
- The procedural star system can be removed once textured night sky is considered
  the production path; for now both coexist via the `has_night_sky_texture` flag.
- A sample equirectangular night sky texture (`data/textures/night_sky.png`) is
  not bundled; users must supply their own HDRI or star-field panorama.

## Skills and instructions used

- `impl-issue` skill
- `src/CLAUDE.md`: one class per file, project-relative includes, Google C++ style
- `src/environment/CLAUDE.md`: module invariants, one-way dependency direction
- `src/editor/CLAUDE.md`: editor as leaf module, nfd file dialogs
- `data/shaders/glsl/CLAUDE.md`: vertex/pixel shader naming, `#include` support
- Global `CLAUDE.md`: conventional commits, history file requirement, cpplint
