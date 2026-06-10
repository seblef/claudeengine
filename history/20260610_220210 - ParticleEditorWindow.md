# ParticleEditorWindow — effect authoring and live preview

## What was built

Added `src/editor/ParticleEditorWindow.h` and `src/editor/ParticleEditorWindow.cpp`, a
new floating editor window for creating and editing `.particles.yaml` composite effects.
Wired into `EditorWindow` under the new **Effects > Particle Editor** menu entry.

## Files changed

| File | Change |
|---|---|
| `src/editor/ParticleEditorWindow.h` | **new** — class declaration |
| `src/editor/ParticleEditorWindow.cpp` | **new** — full implementation |
| `src/editor/EditorWindow.h` | forward-declare `ParticleEditorWindow`; add member + `show_particle_editor_` flag |
| `src/editor/EditorWindow.cpp` | include header, instantiate in constructor, render call in frame loop, "Effects" menu |
| `src/editor/CMakeLists.txt` | add `ParticleEditorWindow.cpp` to `editor` target |

## Layout

Three-column floating window (900 × 620 default):

```
┌─────────────────────────────────────────────────────────────────┐
│ [New] [Load] [Save] [Save As]   filename.particles.yaml         │
├─────────────┬──────────────────────────────┬────────────────────┤
│ Sub-systems │  Sub-system properties       │  Live preview      │
│ > fire_body │  blend_mode / lit / texture  │  (offscreen FBO    │
│   smoke     │  sprite sheet / emitter      │   displayed as     │
│   sparks    │  lifetime / speed / size     │   ImGui::Image)    │
│ [+][-][↑][↓]│  direction / colors          │  [Restart]         │
├─────────────┤                              │  emitter counts    │
│ Embedded    │                              │                    │
│ lights      │                              │                    │
│ > light_0   │                              │                    │
│ [+][-]      │                              │                    │
│ light props │                              │                    │
└─────────────┴──────────────────────────────┴────────────────────┘
```

## Key decisions

### Live preview approach

The issue requested a `GameParticleSystem`-based preview. `GameParticleSystem` requires a
`ParticleSystemTemplate` resource, which can only be loaded from disk (private constructor).
Using raw `ParticleEmitter` instances directly gives the same visual result without needing
to write/reload a YAML file on every property change.

A dedicated `particles::ParticleRenderer` instance (not the main scene's renderer) owns the
preview emitters. The preview renders into an offscreen `kRGBA16F` color + `kDepth24Stencil8`
depth RT, bound before calling `RenderForwardPass()`. The result is displayed via
`ImGui::GetWindowDrawList()->AddImage()` with a Y-flip (OpenGL FBO origin is bottom-left).

The particle forward shader requires UBO slot 2 (SceneInfos). A dedicated `ConstantBuffer`
is filled each frame with preview camera data (`view`, `proj`, `eye_pos`, `time`).

**Limitation**: kGBuffer particles are not shown in the preview (they require the full
deferred pipeline). The preview only renders kAdditive and kAlphaBlend sub-systems.
Lit (kAlphaBlend + `lit=true`) particles will appear unlit because the preview does not bind
a LightInfos CB (UBO slot 4). This is acceptable for effect authoring.

### Stable references for ParticleEmitter

`ParticleEmitter` stores `const ParticleSubSystemDesc&` — a reference, not a copy. If the
working `sub_systems_` vector reallocates (on add/remove), old emitter references would
dangle. The solution: `RebuildPreview()` copies `sub_systems_` into `preview_descs_`
(stable backing store), then creates emitters from `preview_descs_`. The working descs and
the preview descs are kept in sync via `preview_dirty_` — any property change sets the flag
and triggers a rebuild at the start of the next frame.

### Immediate re-instantiation

Any widget change (DragFloat, Checkbox, Combo, InputText) sets `preview_dirty_ = true`,
causing a full rebuild on the next frame. This matches the issue requirement:
"any change immediately re-instantiates the preview". The simulation restarts from t=0,
but one `dt` of simulation runs before the first render, giving immediate visual feedback.

### File operations

- **Load** uses `ParticleSystemTemplate::GetOrLoad()` to reuse the existing YAML parser.
  The descs are copied out and the template is immediately `Release()`'d.
- **Save / Save As** serialize `sub_systems_` and `lights_` using the same YAML schema
  that `ParticleSystemTemplate` reads (verified against `ParseSubSystem` / `ParseLight`).

## Output for next features

- The preview does not show kGBuffer particles. If needed, extend `PreviewRenderer` to
  accept a `ParticleRenderer*` and call `RenderGeometryPass` + `RenderForwardPass` in
  its deferred pipeline.
- Lit alpha-blend particles appear black in the preview. Binding a default `LightInfos` CB
  with a simple directional light would fix this.
- Consider adding orbit-camera mouse controls to the preview (modeled after `MeshPreview`).
- The `ParticleSystemTemplate` registry is not invalidated when the user saves a file with
  the same name as an existing template. If the template is already cached, `GetOrLoad()`
  returns the cached (stale) version. A `ForceReload()` API or cache invalidation on save
  would improve the round-trip workflow.

## Skills and CLAUDE.md instructions applied

- `src/CLAUDE.md`: one class per file, `#include` paths relative to `src/`, Google C++ style.
- `src/editor/CLAUDE.md`: one class per `.h/.cpp` pair, ImGui calls bracketed by
  `NewFrame()`/`Render()`, editor subsystems not singletons.
- Used `cppcheck-suppress unusedStructMember` on all private member variables.
- Followed `MaterialEditorWindow` / `MeshPreview` patterns for the editor window structure,
  NFD file dialogs, and YAML serialization.
