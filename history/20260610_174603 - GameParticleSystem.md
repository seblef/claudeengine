# GameParticleSystem — game object, embedded lights, per-frame update

**Issue**: #452
**Branch**: feat/game-particle-system
**Date**: 2026-06-10

## Changes

### New files
- `src/game/GameParticleSystem.h` / `GameParticleSystem.cpp` — `game::GameObject`
  subclass wrapping a `particles::ParticleSystemTemplate`. Owns one
  `ParticleEmitter` per sub-system and one `renderer::Light` per
  `EmbeddedLightDesc`.

### Modified files
- `src/game/GameObjectType.h` — added `kParticleSystem` enumerator.
- `src/game/GameObjectVisitor.h` — added `Visit(GameParticleSystem&)` pure
  virtual; forward-declared `GameParticleSystem`; re-ordered declarations
  alphabetically.
- `src/game/GameSystem.h` / `.cpp` — `GameSystem` now owns a
  `std::unique_ptr<particles::ParticleRenderer>` created in the constructor
  and registered with `Renderer::SetParticleRenderer()`. `Update()` iterates
  objects and calls `GameParticleSystem::Update(elapsed_time_, dt)` before
  the renderer tick. Added `GetParticleRenderer()` for use by
  `GameParticleSystem::OnAddedToScene / OnRemovedFromScene`.
- `src/game/CMakeLists.txt` — added `GameParticleSystem.cpp`; added `particles`
  to `target_link_libraries` (direct dep, not just transitive through renderer).
- `src/editor/MapSerializer.h` — added no-op `Visit(GameParticleSystem&)` to
  `SerializeVisitor`; particle systems are not serialized to `.map.yaml`.

## Decisions

### ParticleRenderer ownership
`ParticleRenderer.h` documents "Created and owned by GameSystem / EditorSystem".
`GameSystem` is the natural owner: it already controls the full scene lifecycle,
has access to `VideoDevice` via `devices_->GetVideoDevice()`, and constructs
after `Renderer` (so `SetParticleRenderer()` can be called in the constructor
body). A `GetParticleRenderer()` accessor on `GameSystem` lets
`GameParticleSystem` register/unregister emitters without needing a separate
constructor parameter.

### Issue mismatch: ParticleRenderer::Instance()
The issue referenced `ParticleRenderer::Instance()` which doesn't exist (the
class is explicitly not a singleton). The approach above — `GameSystem` owns and
exposes it — matches the documented intent and existing codebase patterns.

### kCircleSpot defaults
`EmbeddedLightDesc` does not carry inner_angle / outer_angle / range / direction
fields for circle-spot lights (only `radius` is present, which is used by OmniLight).
For `kCircleSpot`, the implementation uses defaults (inner=0.2, outer=0.4,
range=20, dir=(0,-1,0)) matching `GameLightDesc` defaults. This can be extended
once `EmbeddedLightDesc` gains those fields.

### video_ stored in GameParticleSystem
`Copy()` must construct a new `GameParticleSystem` with the same template and
video device. Storing `video_` as a member avoids adding a `GameSystem`
`GetVideoDevice()` indirection and keeps the copy operation self-contained.

### MapSerializer visitor
Particle systems are a runtime-only concept (spawned from template keys, not
individual object instances). The `SerializeVisitor::Visit(GameParticleSystem&)`
is a no-op, consistent with `GameTerrain` which has its own root-level
serialization path.

## Next steps
- `EditorSystem` will need its own `ParticleRenderer` (issue #453+).
- Once `EmbeddedLightDesc` gains spot-light params, `CreateEmbeddedLight` in
  `GameParticleSystem.cpp` can use them instead of defaults.
- `MapLoader` / `MapSerializer` may need a `particle_systems:` section to place
  particle systems in levels.

## Skills used
- `impl-issue`

## CLAUDE.md rules applied
- One class per `.h` / `.cpp` pair (`src/CLAUDE.md`)
- Include root is `src/` — all includes are project-relative (`src/CLAUDE.md`)
- No platform or OpenGL headers — only `abstract/` and engine headers
  (`src/game/CLAUDE.md`)
- `GameParticleSystem` does not depend on editor headers (`src/game/CLAUDE.md`)
- Google C++ style guide: `cppcheck-suppress unusedStructMember` on private
  members, `[[nodiscard]]` on getters, named `nullptr` checks in logic
