# Particle Editor — Orbit Camera Control (Issue #464)

## What changed

Added mouse-driven orbit camera to `ParticleEditorWindow`'s live preview panel
(`ParticleEditorWindow.cpp` / `.h`). Previously the camera was fixed; now:

- **Left-drag** orbits azimuth (horizontal) and elevation (vertical, clamped ±89°).
- **Scroll wheel** zooms in/out, distance clamped to [1, 30].

## Files modified

| File | Change |
|------|--------|
| `src/editor/ParticleEditorWindow.h` | Added `core/Vec3f.h` include; four orbit state members (`orbit_azimuth_deg_`, `orbit_elevation_deg_`, `orbit_distance_`, `orbit_center_`); private `UpdatePreviewCamera()` declaration. |
| `src/editor/ParticleEditorWindow.cpp` | Added `<algorithm>`, `<cmath>`; orbit constants; `UpdatePreviewCamera()` implementation; call in `Render()`; mouse input handling in `RenderPreviewColumn()`. |

## Decisions

**Reuse `MeshPreview` pattern verbatim.**  
The same spherical-coordinate formula (`azimuth`, `elevation` → cartesian offset) is used in `MeshPreview::UpdateCamera()`. Reusing it keeps the two preview panels consistent and avoids inventing a new interaction model.

**Fixed orbit centre at `{0, 1.5, 0}`.**  
Particles are emitted from the origin and typically rise upward, so centering slightly above ground makes the default view useful. Unlike meshes, particles have no bounding box, so dynamic centering is not applicable.

**Orbit state initialised to azimuth=30°, elevation=20°, distance=7.**  
These produce the same approximate view as the previous static `SetPosition({3,3,6})` / `SetTarget({0,1.5,0})` setup.

**`UpdatePreviewCamera()` called every frame, not only on input.**  
The method is cheap (trig + three `Camera` setter calls). Calling it unconditionally in `Render()` keeps the logic simple — no dirty flag needed for camera state.

## Key constants

```cpp
kOrbitSensitivity = 0.4f   // degrees per pixel (matches MeshPreview)
kZoomSensitivity  = 0.5f   // world units per scroll tick
kZoomMin          = 1.f
kZoomMax          = 30.f
```

## Skills / CLAUDE.md instructions used

- `impl-issue` skill
- `src/CLAUDE.md`: one class per file, Google style, include what you use
- `src/editor/CLAUDE.md`: one class per `.h`/`.cpp` pair, ImGui calls bracketed

## PR

https://github.com/seblef/claudeengine/pull/492 — closes #464
