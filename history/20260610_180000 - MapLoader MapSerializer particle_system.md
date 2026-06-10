# MapLoader & MapSerializer — particle_system object type

**Date:** 2026-06-10
**Branch:** feat/map-particle-system-serialization
**Issue:** #453
**PR:** closes #453

## Changes

### `src/game/MapLoader.cpp`

- Added includes `game/GameParticleSystem.h` and `particles/ParticleSystemTemplate.h`.
- Added `ParseParticleSystem(node, video)` in the anonymous namespace: calls
  `ParticleSystemTemplate::GetOrLoad(tmpl_name, video)`, logs a `WARNING` and
  returns `nullptr` if the template is missing, then builds a `GameParticleSystem`,
  releases the extra `GetOrLoad` ref, sets name and world transform, and returns.
- Added `type == "particle_system"` branch in the objects loop; null returns from
  `ParseParticleSystem` are silently skipped (same pattern as `terrain`).

### `src/editor/MapSerializer.h`

- Changed the empty stub `void Visit(game::GameParticleSystem&) override {}`
  to a proper declaration `void Visit(game::GameParticleSystem& particle_system) override;`.

### `src/editor/MapSerializer.cpp`

- Added includes `game/GameParticleSystem.h` and `particles/ParticleSystemTemplate.h`.
- Implemented `SerializeVisitor::Visit(GameParticleSystem&)`: emits a YAML mapping
  with keys `name`, `type: particle_system`, `template` (the basename from
  `tmpl->GetId()`), and `transform` (flat 16-element row-major matrix via
  `EmitTransform`, matching all other object types).

## Decisions

- **Transform format**: kept the existing flat 16-element `EmitTransform` / `core::ParseMat4`
  encoding used by every other object type, rather than a decomposed
  position/rotation/scale as shown in the issue spec. This is the format that already
  round-trips correctly through the loader.
- **Null guard in loader**: `ParseParticleSystem` returns `nullptr` on missing template
  (matches the `terrain` branch). The warning is logged inside the helper, not at the
  call site, keeping the loop tidy.
- **No changes to `GameObjectType`**: `kParticleSystem` already exists; no enum change needed.

## Notes for next contributions

- `GameParticleSystem` does not appear in the editor's `ObjectsPanel` or `PropertiesPanel`
  yet — adding UI to place, select and inspect particle systems is the logical next step.
- `MapSerializer::Load()` does not need special-casing for particle systems: `MapLoader`
  returns a fully instantiated `GameParticleSystem`; `EditorScene::AddDynamicObject` accepts
  any `GameObject`, so it flows in automatically.

## Skills / CLAUDE.md instructions followed

- `src/CLAUDE.md`: one class per file, include paths relative to `src/`, Google C++ style.
- `src/game/CLAUDE.md`: follow `GetOrLoad` + `Release` pattern for ref-counted resources.
- `src/particles/CLAUDE.md`: template key is the basename without extension.
- Root `CLAUDE.md` git workflow: checkout dev → pull → create branch → implement → cpplint → commit → PR.
