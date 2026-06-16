# Particle Editor Scene Sync — fix #576

## Problem

When a particle system template was edited and saved in the Particle Editor window,
the changes were written to disk but:

1. The in-memory `ParticleSystemTemplate` (a reference-counted singleton) was never
   updated — it still held the old sub-system and light descriptors from the initial
   file load.
2. Every `GameParticleSystem` in the scene had already built its `ParticleEmitter`
   and `renderer::Light` instances from the stale template data at construction time.
   Even if the template had been refreshed, the live emitters would have continued
   running the old simulation.

## Changes

### `particles/ParticleSystemTemplate` — `Reload()` method

Added a public `Reload()` method that clears `sub_systems_` and `lights_` and
re-parses the YAML from disk (the same path the constructor uses). This re-uses the
existing anonymous-namespace parse helpers (`ParseSubSystem`, `ParseLight`) so there
is no duplication of the loading logic.

Rationale: re-reading from disk is the simplest approach given the file was just
written by the editor. A direct in-memory update via setters would require exposing
mutation APIs on an otherwise immutable resource.

### `game/GameParticleSystem` — `ReloadFromTemplate()` method

Added a public `ReloadFromTemplate()` method that:

1. Checks whether the object is currently registered with the renderer
   (`renderables_` is non-empty ↔ in-scene).
2. If in-scene, removes renderables and lights from the renderer.
3. Destroys all emitters and lights.
4. Rebuilds emitters from `template_->GetSubSystems()` and lights from
   `template_->GetLights()` (same logic as the constructor).
5. If was in-scene, re-registers the new renderables and lights, then calls
   `OnWorldTransformUpdated()` to apply the current world matrix.

### `editor/ParticleEditorWindow` — `SetOnTemplateSaved` callback

Added a `std::function<void(const std::string&)>` callback registered via
`SetOnTemplateSaved()`. It is fired after every successful `SerializeToFile()` call
(in both `Save()` and `SaveAs()`). The argument is the template basename extracted
with `path.stem().stem().string()` (e.g. `"firecamp01"` from
`firecamp01.particles.yaml`).

Panels must remain pure UI per the editor CLAUDE.md invariant — the callback keeps
the wiring responsibility in `EditorWindow`.

### `editor/EditorWindow` — wires the callback

In the constructor, after the other `particle_editor_->…` wiring:

```cpp
particle_editor_->SetOnTemplateSaved([this](const std::string& name) {
    ParticleSystemTemplate* tmpl = ParticleSystemTemplate::Get(name);
    if (!tmpl) return;                   // not in memory → nothing to update
    tmpl->Reload();
    for (game::GameObject* obj : scene_->GetObjects()) {
        if (obj->GetType() != GameObjectType::kParticleSystem) continue;
        auto* ps = static_cast<GameParticleSystem*>(obj);
        if (ps->GetTemplate() == tmpl) ps->ReloadFromTemplate();
    }
});
```

`ParticleSystemTemplate::Get()` (non-AddRef'd lookup) is used because we don't want
to extend the lifetime here; `Reload()` operates on the existing instance.

## Key decisions

| Decision | Rationale |
|---|---|
| Re-parse from disk rather than push from editor memory | Single source of truth is the YAML file; avoids exposing mutable setters on the template. |
| Callback over direct scene pointer | Keeps `ParticleEditorWindow` as pure UI with no knowledge of the scene hierarchy. |
| `renderables_.empty()` to detect in-scene state | `renderables_` is the only flag available without adding a boolean; it is set in `OnAddedToScene` and cleared in `OnRemovedFromScene`. |
| Rebuild unconditionally (not diff) | Sub-system counts can change; diffing individual fields is error-prone. Full rebuild is simpler and correct. |

## Output / keep in mind

- If the particle editor saves a template that has no live instances in the scene,
  `Get()` may return `nullptr` (the template might not be loaded at all). The
  callback handles this gracefully with an early return; the YAML file is still
  updated on disk and will be correct on next load.
- The stash from branch `feat/particle-fire-extensions-555` was present at
  checkout; it was stashed before switching to `dev` and is unrelated to this fix.

## Skills / instructions followed

- `impl-issue` skill (branch from `dev`, conventional commit, PR to `dev`)
- `src/CLAUDE.md`: one class per file, Google C++ style
- `src/editor/CLAUDE.md`: panels as pure UI, wiring in `EditorWindow`
- `src/game/CLAUDE.md`: `OnAddedToScene`/`OnRemovedFromScene` lifecycle contract
- `src/particles/CLAUDE.md`: no platform headers, one class per file
