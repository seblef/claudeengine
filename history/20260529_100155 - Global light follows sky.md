# Global Light Follows Sky

**Issue:** #348 — Global light is not updated according to sun/moon position and color  
**Branch:** `fix/global-light-follows-sky-348`

## Problem

The global scene directional light (sun/moon) was static: its direction, color,
and intensity never changed regardless of the current time of day or the sky
simulation. The sky renderer computed a correct sun position from `WorldTime`,
but `EnvironmentEditorPanel` only forwarded the time-of-day value to
`SkyRenderer` and `WaterRenderer` — the `GlobalLight` was never touched.

## Root Cause

`EnvironmentEditorPanel::Tick()` advanced `world_time_` and called
`SetSkyWorldTime()` / `SetWaterSkyWorldTime()`, but did not update the scene's
global light. Similarly, the "Time of day" slider in the UI updated the sky but
left the light unchanged.

## Changes

### `src/editor/EnvironmentEditorPanel.h`

Added a private helper declaration:

```cpp
void UpdateGlobalLightFromSky();
```

### `src/editor/EnvironmentEditorPanel.cpp`

**New constants** (file-scope anonymous namespace):

| Constant | Value | Meaning |
|---|---|---|
| `kSunMaxIntensity` | 1.2 | Full-noon sun intensity |
| `kMoonIntensity` | 0.08 | Fixed dim moonlight |
| `kSinMaxElevation` | 0.7071 | sin(45°) — latitude 45° N max solar elevation |

**`UpdateGlobalLightFromSky()`** — new helper:

1. Guards on `world_time_` and `scene_` being non-null.
2. Reads `IsDaytime()` to choose sun vs moon.
3. Gets the celestial direction from `GetSunDirection()` / `GetMoonDirection()`
   (these return *toward* the body, positive Y = above horizon).
4. **Negates** it to produce `GameLightDesc.direction` (the ray from source to
   surface — confirmed by `global_light_ps.glsl`: `L = normalize(-direction)`).
5. **Sun**: warm white `(1.0, 0.95, 0.8)`, intensity scales with elevation:
   `kSunMaxIntensity × max(0, sky_toward.y) / sin(45°)`.
6. **Moon**: cool blue-white `(0.45, 0.5, 0.8)`, constant `kMoonIntensity`.
7. Preserves shadow settings (cast_shadow, resolution, bias) from the existing
   global light desc — only direction, color and intensity are overridden.
8. Calls `scene_->SetGlobalLightDesc(desc)` which immediately syncs the live
   `GlobalLight` in the renderer.

**Call sites added:**

- `Tick()` — called after every time advance (runs every editor frame when sky is active).
- `RenderTimeSection()` — called when the time-of-day slider is dragged.
- `EnableSky()` — called once when sky is first enabled, to initialise the light
  state from the starting time instead of leaving it stale.

## Decisions

- **Direction convention**: verified in `global_light_ps.glsl` that `direction`
  is negated before computing the Blinn-Phong `L` vector. `WorldTime` returns
  "toward sun" (positive Y above horizon), so the negation is mandatory.
- **Preserve shadow settings**: direction/color/intensity are sky-driven; shadow
  resolution and bias are user preferences that should not be overridden by the
  sky system.
- **Ambient color**: left unchanged. The issue mentions only direction, color
  and intensity; ambient is a separate artistic parameter.
- **Sun intensity scaling**: linear with sin(elevation). At the horizon
  (elevation = 0) intensity = 0 — appropriate because Lambertian diffuse
  already falls to near-zero when L ≈ tangent anyway.

## Output to keep in mind

- Sky-driven light direction/color/intensity is reset every tick when sky is
  enabled; manual edits in the PropertiesPanel are overridden each frame. If
  manual overrides are desired in the future, a "sky-driven" toggle should be
  added to `EnvironmentDesc`.
- The sun elevation factor uses `sin(45°)` as the normaliser. If the maximum
  elevation constant in `WorldTime.cpp` changes, `kSinMaxElevation` here must
  be updated in sync.
- The transition at sunrise/sunset is abrupt (hard switch between sun and moon
  at `IsDaytime()` = false). A smooth blend zone could be added in the future.

## Skills and instructions used

- `impl-issue` skill
- `src/CLAUDE.md` — one class per file, Google C++ style, no unnecessary comments
- `src/editor/CLAUDE.md` — editor is the dependency graph leaf; one class per pair
- `src/environment/CLAUDE.md` — environment must not depend on renderer
- `CLAUDE.md` (root) — git workflow, history file, PR rules
