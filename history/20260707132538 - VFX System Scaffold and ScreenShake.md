# VFX System Scaffold and ScreenShake

Implements GitHub issue #827: `vfx/` module scaffold (`VFXSystem`, `IVFXEffect`) plus the first concrete effect, `ScreenShake`.

## Summary of changes

### New `src/vfx/` module

- **`IVFXEffect.h`** — interface for a self-contained, self-expiring effect: `Play(world_pos, direction)`, `Update(dt)`, `IsFinished()`.
- **`VFXSystem.h/.cpp`** — `core::Singleton<VFXSystem>` (same pattern as `Renderer`, `PhysicsSystem`, `GameSystem`). `Spawn()` takes ownership of an `IVFXEffect`, calls `Play()` immediately, and returns a non-owning pointer. `Update(dt)` ticks every active effect and reaps finished ones each frame — mirrors `SoundManager`'s reap-in-`Update()` pattern (owned in a container, expiry detected centrally) since no `IsFinished()`/self-expiring precedent existed elsewhere in the codebase.
- **`ScreenShake.h/.cpp`** — concrete `IVFXEffect`. `Play()` computes a magnitude that falls off with inverse distance from the active camera (`base_magnitude_ / max(distance, 1)`), then calls the new `ICameraController::ApplyShake(magnitude, duration)` once. It only tracks elapsed time internally so `VFXSystem` knows when to reap it — the actual per-frame decay lives in the camera controller, matching `WRECKONING.md` §12: *"ChaseCameraController applies it as a decaying sinusoidal offset on its output transform."*
- `vfx/CMakeLists.txt` links `core abstract game renderer particles audio physics` (all PUBLIC, matching the loose linking style already used by `game/CMakeLists.txt`). Added to `src/CMakeLists.txt` **after** `game`, and to `app/CMakeLists.txt`'s `target_link_libraries` (the executable wasn't picking up the new static lib without this — link-only dependency, no `#include` needed in other modules).

### `ICameraController` — new `ApplyShake` pure virtual

Added `virtual void ApplyShake(float magnitude, float duration_seconds) = 0;`. This is the only viable communication channel per the issue's constraint ("must not hardcode `ChaseCameraController`"): the codebase has no generic gameplay event bus (`core::EventManager` is strictly platform input events, consumed solely by `GameSystem`), so a new interface method is the established pattern here (mirrors how the existing 3-method interface is small and directly called by whoever owns the controller).

This interface change means **every** `ICameraController` implementation must supply it:
- `ChaseCameraController` and `FPSCameraController` — real decaying shake (see below).
- `editor::EditorCameraController` — added as a no-op inline override with a comment explaining why (the editor viewport is never subject to gameplay shake). Found this third implementation only when `wreckoning_editor` failed to link with "abstract class" errors — worth flagging for next time: **grep all `ICameraController` implementers before extending the interface**, don't assume the two in `game/` are the only ones.

### `game::CameraShake` (new small helper, shared by both gameplay controllers)

Both `ChaseCameraController` and `FPSCameraController` need identical decay math (linear decay to zero, three independent sine waves for lateral/vertical/roll offsets). Rather than duplicate ~15 lines of non-trivial math verbatim in two files, factored it into `game/CameraShake.h/.cpp` — a small state-only helper (`Trigger(magnitude, duration)`, `Advance(dt)`, `GetLateralOffset()/GetVerticalOffset()/GetRollOffset()`). Each controller owns a `CameraShake shake_;` member, calls `shake_.Trigger(...)` from its `ApplyShake()` override, and blends the three offsets into its own right/up/forward basis right before building the final transform in `Update()`.

### `game::GameSystem` — two new getters

`GetCameraController()` and `GetActiveCamera()` (both `[[nodiscard]]`, inline). Needed because `ScreenShake::Play()` (in `vfx/`, which is allowed to depend on `game/`) needs read access to the active controller and camera to compute distance falloff and trigger the shake. Neither existed before — only setters did.

### `app/main.cpp` wiring

- `new vfx::VFXSystem();` created after the audio setup block (last among the engine singletons `vfx` depends on: `game`, `renderer`, `particles`, `audio`).
- Ticked once per frame after `game.Update()`, using its own `prev_vfx_elapsed` tracked separately from the pre-existing `prev_elapsed` (which is only updated inside the `if (map_world_time || map_wind_system)` block and would otherwise not be available unconditionally). Deliberately did not touch the existing world/wind timing logic — smallest possible diff.
- `vfx::VFXSystem::Shutdown();` called first, right after the main loop, before any of its dependencies (`physics::PhysicsSystem`, `game::GameSystem`, `renderer::Renderer`) are torn down.

### Tests

`tests/vfx/VFXSystemTest.cpp` — 3 tests using a local `NoOpEffect` stub (mirrors the `StubObject` pattern in `tests/game/GameObjectHierarchyTest.cpp`): spawn registers an active effect, `Update()` reaps a finished effect, multiple effects expire independently. Compiles `src/vfx/VFXSystem.cpp` directly and links only `core` (no `game`/`renderer`/`audio` pulled in) — `VFXSystem` itself has zero dependencies beyond `core::Vec3f`/`core::Singleton`, so this stays a fast, isolated unit test. This satisfies the issue's acceptance criterion ("a test/debug call can spawn a no-op `IVFXEffect` and see it expire via `IsFinished()`") without adding a runtime debug key.

## Verification performed

- `cpplint` clean on all new/changed files (project `CPPLINT.cfg` filters applied).
- Full rebuild of `wreckoning`, `wreckoning_editor`, `vfx_tests`, `game_tests` — all succeed.
- `ctest` run for `VFXSystemTest` (3/3) and `GameObjectHierarchyTest` (13/13) — all pass.
- **Not done**: interactive/visual confirmation that `ScreenShake` perturbs the live camera in a running window (no display available in this environment). The logic path (`ScreenShake::Play` → `ICameraController::ApplyShake` → `CameraShake::Advance` blended into the output transform) is exercised end-to-end by types but not visually verified — flag this for manual QA before merge, or as a follow-up when `VFXExplosion` lands and can trigger it in a real map.

## Output to keep in mind for next VFX issues

- `VFXExplosion`/`VFXFire`/`VFXScrape`/`VFXElectricity` can now depend on `vfx::VFXSystem::Instance().Spawn(...)` and the `IVFXEffect` interface directly; no further scaffolding needed.
- `VFXExplosion`'s shockwave step needs `physics::PhysicsSystem::SphereOverlap` — **this does not exist yet** (only `Raycast` is implemented). Flagging now so it isn't a surprise when that issue starts.
- Any future addition to `ICameraController` must account for **three** implementers: `ChaseCameraController`, `FPSCameraController`, and `editor::EditorCameraController` — grep first.
- `game::CameraShake` is reusable if a future effect needs a different decay shape (e.g. impact-punch vs. rumble) — either parameterize it or add a sibling helper; don't inline more shake math into the controllers.

## Propositions

**CLAUDE.md improvements** (not applied, proposing only):
- `src/game/CLAUDE.md`'s module table doesn't list `CameraShake` or the `ApplyShake` contract on `ICameraController` — worth a row once a second consumer (an explosion effect) exists, to avoid re-deriving "why does this exist" from git blame.
- No top-level or `src/CLAUDE.md` change needed — neither enumerates modules exhaustively enough to require a `vfx/` entry.

**README.md**: no build/install process changes (no new third-party library, `vfx/` follows the existing `add_subdirectory` + `CMakeLists.txt` template) — no proposal needed.

**Specification questions**: none — the issue, `WRECKONING.md` §12, and the existing `SoundManager`/`Singleton` precedents were sufficient to resolve every design decision (interface shape, dependency wiring, shake math ownership) without needing to ask.

## Skills used

- `impl-issue` (invoked via `/impl-issue 827`).

## CLAUDE.md instructions especially taken into account

- `src/CLAUDE.md`: one class per `.h`/`.cpp`, new modules need `CMakeLists.txt` + `src/CMakeLists.txt` entry, project-relative `#include` paths.
- `src/game/CLAUDE.md`: "Components ... call into `SoundManager`, `ParticleRenderer`, etc. directly — they do not go through `GameSystem`" — informed keeping `CameraShake` as a plain non-scene-object helper rather than a `GameObject`.
- `src/particles/CLAUDE.md` dependency-graph note (`game → renderer → particles`) — confirmed `vfx`'s link set doesn't create a cycle.
- Root `CLAUDE.md` git workflow: branched from freshly-pulled `dev`, ran `cpplint`, conventional-commit message, PR to `dev` with closing keyword.
