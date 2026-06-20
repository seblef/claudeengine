# Overlay Rendering Options (Issue #684)

**Branch**: `feat/overlay-rendering-options-684`  
**PR**: #688 → `dev`

## What was done

Added per-type wireframe overlay toggle checkboxes to `RenderingSettingsPanel`. Each object type (lights, sounds, particle systems, player starts) now has its own enable flag, saved and restored from `config.yaml`.

## Files changed

| File | Change |
|---|---|
| `src/renderer/WireframeRenderer.h` | Added `light_gizmos_enabled_`, `particle_gizmos_enabled_` fields; `SetLightGizmosEnabled`, `SetParticleGizmosEnabled`, `AreLightGizmosEnabled`, `AreParticleGizmosEnabled` |
| `src/renderer/Light.cpp` | `Light::Enqueue()` now checks `AreLightGizmosEnabled()` instead of `AreGizmosEnabled()` |
| `src/renderer/ParticleRenderable.cpp` | `ParticleRenderable::Enqueue()` now checks `AreParticleGizmosEnabled()` instead of `AreGizmosEnabled()` |
| `src/editor/RenderingSettingsPanel.h` | Four overlay bool fields + getters/setters |
| `src/editor/RenderingSettingsPanel.cpp` | New "Overlay" `CollapsingHeader` with four checkboxes |
| `src/editor/EditorViewport.cpp` | Propagates panel flags to `WireframeRenderer` before `Renderer::Update()`; guards `EnqueuePlayerStartGizmos` / `EnqueueSoundEmitterGizmos` |
| `src/editor/EditorWindow.h` | `SaveOverlaySettings()` declaration |
| `src/editor/EditorWindow.cpp` | Loads `editor.overlay` from config on startup; calls `SaveOverlaySettings()` in destructor; implements `SaveOverlaySettings()` |

## Key decisions

### Per-type flags in WireframeRenderer, not in the editor

`Light::Enqueue()` and `ParticleRenderable::Enqueue()` live in the `renderer` module, which cannot depend on `editor`. The per-type flags were therefore added to `WireframeRenderer` (already the gateway for all gizmo state) and set from `EditorViewport::Render()` each frame before `Renderer::Update()` is called. The existing `gizmos_enabled_` master switch is ANDed with each per-type flag (`AreLightGizmosEnabled = gizmos_enabled_ && light_gizmos_enabled_`), preserving the ability to disable all gizmos globally.

### Flag update timing

The per-type flags must be written to `WireframeRenderer` *before* `Renderer::Update()` because lights and particles push their wireframe segments during the visibility pass inside that call. Player start and sound emitter gizmos are enqueued by the editor *after* `Renderer::Update()`, so they are simply guarded by an `if` in `EditorViewport::Render()`.

### Separate save function

`SaveOverlaySettings()` is a new, separate function called in the destructor alongside the existing `SavePhysicsDebugSettings()`. This mirrors the existing pattern and avoids merging two unrelated concerns into one function.

## Config format

```yaml
editor:
  overlay:
    lights: true
    sounds: true
    particles: true
    player_starts: true
```

## Skills and CLAUDE.md rules applied

- `src/CLAUDE.md`: one class per `.h`/`.cpp`; `#include` paths relative to `src/`; Google C++ style
- `src/editor/CLAUDE.md`: panels are pure UI — no editing logic inline; dependency invariant (`editor` is the leaf, `renderer` must not import `editor`)
- Conventional commit message format
- cpplint passed on all modified files
