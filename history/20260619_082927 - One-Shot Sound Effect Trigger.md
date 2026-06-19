# One-Shot Sound Effect Trigger

**Issue**: #667
**Branch**: `feat/sound-effect-component-667`
**Date**: 2026-06-19

## Summary

Added a fire-and-forget sound API (`SoundManager::PlayOnce`) and a lightweight attachable component (`SoundEffectComponent`) for triggering one-shot positional sound effects (footsteps, explosions, UI events).

## Changes

### `audio/SoundManager` — new `PlayOnce` method

Added a `void` overload that wraps `PlaySound` with `loop=false` and discards the handle:

```cpp
void PlayOnce(Sound* sound, const core::Vec3f& position,
              int priority = 0, float gain = 1.0f);
```

**Rationale**: `PlaySound` is `[[nodiscard]]` and returns a `VirtualSoundInstance*` handle. For genuine fire-and-forget use cases (footsteps, UI clicks), forcing the caller to capture and ignore a handle is noisy and misleading. `PlayOnce` communicates intent clearly — if early stopping is needed, `PlaySound` is still the right call. The implementation calls `PlaySound` internally and uses `(void)` to suppress the nodiscard warning.

### `game/SoundEffectComponent` — new component class

A non-`GameObject` component that can be embedded in any game entity:

```cpp
SoundEffectComponent fx("explosion", sound_manager, resource_manager);
// Later, at trigger site:
fx.Trigger(entity_world_pos);
```

**Rationale for component vs scene object**: The issue describes an "attachable" pattern — not every explosion emitter is a persistent scene object. `SoundEffectComponent` is a value member, not a `GameObject` subclass. This keeps the scene graph clean (no `kSoundEffect` enum clutter, no visitor overloads) and lets any entity own multiple components for different sounds (footstep + land + hurt).

**Lifecycle**: The component loads the `Sound*` via `ResourceManager::LoadSound()` in its constructor (AddRef'd) and releases it in the destructor. This mirrors `GameSoundEmitter`. Trigger is safe to call before or after the entity enters the scene.

**Null safety**: If either `SoundManager*` or `ResourceManager*` is null, all operations are silent no-ops. This matches `GameSoundEmitter`'s pattern for audio-less contexts (unit tests, headless server).

## Key decisions

| Decision | Alternative considered | Rationale |
|---|---|---|
| `PlayOnce` returns `void` | Return `VirtualSoundInstance*` | Removing the handle from the API communicates fire-and-forget intent; callers who want control use `PlaySound` |
| `SoundEffectComponent` is not a `GameObject` | Subclass `GameObject` as `kSoundEffect` | Components attach to entities; scene objects have independent existence. Adding a type enum value + visitor override for a purely transient event is overhead without benefit |
| Position passed to `Trigger()` at call time | Store position in the component | Entities move; storing position would require synchronization. Caller always has the current world transform |

## Output to keep in mind

- `SoundManager::PlayOnce` can be used from `GameSoundEmitter` itself if it ever needs to emit a one-shot (e.g., start/stop effects).
- `SoundEffectComponent` does not integrate with the editor or `MapLoader` — it is purely a runtime component. If editor support is needed in the future, a `GameSoundEffect` scene object variant would be the right approach (similar to `GameParticleSystem`).
- The audio CLAUDE.md already notes: "Suspended instances will restart from the beginning when they re-acquire a hardware source". This still applies to `PlayOnce` — if the pool is exhausted, the instance is suspended and will restart. For very short one-shots (< 100ms) this could produce double triggers. Consider filtering by `VirtualSoundState` on re-acquire in a future iteration.

## Skills / CLAUDE.md used

- `impl-issue` skill (workflow)
- `src/CLAUDE.md`: one class per .h/.cpp, Google style, include root is `src/`
- `src/audio/CLAUDE.md`: dependency graph, PlaySound usage, lifecycle invariants
- `src/game/CLAUDE.md`: dependency graph, component ownership conventions
- `src/CLAUDE.md`: `cppcheck-suppress unusedStructMember` on private members
