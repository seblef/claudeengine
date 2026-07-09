# CLAUDE.md — vfx module

## Role

The `vfx` module is the coordination layer for composited visual effects.
`particles/` only knows about raw emitters; `vfx/` owns the lifetime of
higher-level, multi-element effects built on top of them — an explosion is a
particle burst + a renderer light + a physics query + a camera shake, all
started and torn down together.

## Dependency graph

```
vfx/ → game/ → renderer/ → particles/ → audio/
```

`vfx/` must not be depended on by `game/`, `renderer/`, `particles/`, or
`audio/` — if a `game/`-level callback (e.g. a vehicle contact event) needs
to trigger a `vfx/` effect, wire it through a `game/`-owned interface and a
non-owning setter (see `IVehicleScrapeListener` / `VehicleScrapeEffect`
below), never a direct `#include "vfx/..."` from `game/`.

## Module structure

| File(s) | Responsibility |
|---|---|
| `IVFXEffect.h` | Interface every effect implements: `Play(worldPos, direction)`, `Update(dt)`, `IsFinished()` |
| `VFXSystem` | Singleton; `Spawn()` takes ownership of an effect, plays it immediately, and reaps it once `IsFinished()` |
| `VFXExplosion` / `VFXExplosionDesc` | Fireball/smoke burst + point-light flash + physics shockwave + screen shake |
| `VFXElectricity` / `VFXElectricityDesc` | One-shot arc between two world-space points, visualised as a chain of spark bursts along the segment |
| `VFXScrape` | Continuous directional sparks + looping screech, driven by an external `UpdateContact()` call per physics step |
| `VehicleScrapeEffect` | `game::IVehicleScrapeListener` implementation; owns the persistent spark template and drives a `VFXScrape` from vehicle contact events |
| `VFXFire` | Looping fire + smoke attached to a moving target, driven by an external `SetWorldTransform()` call every frame; carries an unwired heat-distortion stub flag |
| `VehicleFireEffect` | `game::IVehicleDamageListener` + `game::IVehicleFireListener` implementation; owns the persistent fire template and drives a `VFXFire` from vehicle damage-threshold crossings, following the body every frame |
| `VehicleWreckEffect` | `game::IVehicleWreckListener` implementation; spawns a one-shot `VFXExplosion` + wreck sound the instant `game::GameVehicle` reports the vehicle wrecked |
| `ScreenShake` | Fire-and-forget camera shake; signals `game::ICameraController::ApplyShake()`, falloff by inverse distance from the camera |

## Key patterns

### The `IVFXEffect` contract

`Play()` starts the effect once; `Update(dt)` advances it every frame;
`IsFinished()` is polled by `VFXSystem` so the instance can be reaped. Most
effects are genuinely fire-and-forget (`VFXExplosion`, `ScreenShake`): `Play()`
does all the real work and `Update(dt)` only ticks decay/particle simulation.
`VFXScrape` and `VFXFire` are the exception — they're stateful and driven by
an external call (`UpdateContact()` / `SetWorldTransform()` respectively) for
as long as the effect should keep running; `Update(dt)` on its own never
decides when either effect ends (`Stop()` does).

### Composition: an effect can spawn sibling effects

`VFXExplosion` doesn't reimplement camera shake — it constructs a
`ScreenShake`, configures it, and `VFXSystem::Instance().Spawn()`s it as an
independent effect alongside itself. The two are not coupled: the shake keeps
running (and gets reaped by `VFXSystem`) on its own timeline even after the
explosion that triggered it has self-expired. Prefer this pattern — spawning
an existing simpler effect — over duplicating logic (e.g. distance-falloff
math) that another effect already owns.

### External template ownership

Effects that use `particles::ParticleSystemTemplate` (`VFXScrape`,
`VFXExplosion`, `VFXElectricity`) take it as a **non-owning constructor
parameter**, never load
it themselves via `GetOrLoad()`. `core::Resource::Release()` deletes the
template the instant its ref count hits zero, so an effect that is
constructed-and-destroyed per trigger (or even just short-lived) would
reload it from disk every single time if it owned the reference. The caller
is responsible for loading the template once (typically via a long-lived
owner — `VehicleScrapeEffect` holds `sparks` for a vehicle's whole lifetime)
and holding it for as long as that effect might be triggered. Every effect
using this pattern must tolerate a `nullptr` template (no particles emitted,
everything else still fires normally) — this is what lets unit tests
construct effects with no `Renderer` instanced.

### `GameLight` vs. a bare `renderer::Light`

When an effect needs a transient renderer light (e.g. an explosion's flash),
spawn a real `game::GameLight` and register/unregister it via
`game::GameSystem::Instance().AddObject()`/`RemoveObject()` — not a bare
`renderer::OmniLight` added directly to `Renderer`. `GameLight` already owns
the light's scene lifecycle (`OnAddedToScene`/`OnRemovedFromScene`); reaching
around it to call `Renderer::AddRenderable()` directly would duplicate that
registration logic and diverge from how every other light in the engine is
managed.

### Graceful degradation with no singletons instanced

Every effect must no-op safely, not crash, when `renderer::Renderer`,
`game::GameSystem`, `physics::PhysicsSystem`, or `vfx::VFXSystem` aren't
instanced — guard each optional step with that singleton's `IsInstanced()`.
This is what makes effects unit-testable without standing up the whole
engine; see any `*Test.cpp` in `tests/vfx/` for the expected coverage shape
(lifecycle with everything absent, plus one narrowly-scoped test per
singleton-gated behaviour).

## Guidelines

Follow all rules in `src/CLAUDE.md`. Additionally:
- One class per `.h` / `.cpp` pair; `*Desc` structs (e.g. `VFXExplosionDesc`)
  may be header-only.
- New effect `.cpp` files need an entry in `src/vfx/CMakeLists.txt`'s
  `add_library(vfx STATIC ...)`, plus a matching entry in
  `tests/vfx/CMakeLists.txt`'s `target_sources()` if any test exercises it.
