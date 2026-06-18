# GameSoundEmitter — Issue #666

## Summary

Added `GameSoundEmitter`, a `GameObject` subclass that loops a 3D positional
sound at its world-space location. The emitter is serialized in `.map.yaml`,
registers a `VirtualSoundInstance` with the `SoundManager` on scene entry, and
unregisters on scene exit. Position is forwarded to the `VirtualSoundInstance`
whenever the world transform changes.

## Changes

### New files

| File | Purpose |
|---|---|
| `src/game/GameSoundEmitter.h` | Class declaration |
| `src/game/GameSoundEmitter.cpp` | Implementation |

### Modified files

| File | Change |
|---|---|
| `src/game/GameObjectType.h` | Added `kSoundEmitter` enum value |
| `src/game/GameObjectVisitor.h` | Added `GameSoundEmitter` forward declaration and `Visit(GameSoundEmitter&)` |
| `src/game/MapLoader.h` | Added optional `audio::SoundManager*` and `audio::ResourceManager*` params to `Load()` (defaulting to nullptr for backwards compat) |
| `src/game/MapLoader.cpp` | Added `ParseSoundEmitter()` and `sound_emitter` dispatch case |
| `src/game/GameSystem.h` | Added `SetSoundManager()` and `sound_manager_` member |
| `src/game/GameSystem.cpp` | Ticks `SoundManager::SetListenerTransform()` and `SoundManager::Update(dt)` each frame when set |
| `src/game/CMakeLists.txt` | Added `GameSoundEmitter.cpp`, linked `audio` library |

## Key decisions

### Position via OnWorldTransformUpdated, not a per-frame loop

The issue says "Position updated each frame from game object transform". Rather
than polling in a `GameSystem` loop every frame, position is forwarded in
`OnWorldTransformUpdated()` — which is called whenever `SetWorldTransform()` is
invoked. For static emitters this is once at load time; for moving emitters it
fires only when the transform actually changes. This avoids an unnecessary
per-object dispatch in the hot game loop.

### Null-safe audio context

`GameSoundEmitter` accepts null `SoundManager*` / `ResourceManager*` and
degrades gracefully to a silent no-op. `MapLoader::Load()` gains optional audio
params (default nullptr) so all existing callers remain unchanged.

### SoundManager ticked in GameSystem

`GameSystem::SetSoundManager()` registers the manager. Each frame,
`GameSystem::Update()` calls `SetListenerTransform(camera_world_transform)` then
`Update(dt)`, mirroring how `PhysicsSystem` is driven. Callers in `main.cpp`
need to create a `SoundManager`, call `game.SetSoundManager(&mgr)`, and pass it
to `MapLoader::Load()`.

### Resource loading through ResourceManager

Follows `audio/CLAUDE.md` — sound is loaded via `ResourceManager::LoadSound()`
(not `Sound::GetOrLoad()` directly). The returned AddRef'd pointer is held for
the emitter's lifetime and released in the destructor.

## YAML format

```yaml
objects:
  - type: sound_emitter
    name: Waterfall Ambient
    sound: waterfall          # stem of data/sounds/waterfall.sound.yaml
    volume_scale: 0.8         # optional, default 1.0
    transform:
      position: [10, 2, 5]
      rotation: [0, 0, 0]
```

## Output to keep in mind

- `GameObjectVisitor` now has a pure-virtual `Visit(GameSoundEmitter&)` — any
  existing concrete visitor implementations outside this repo will need to add
  this override.
- `main.cpp` does not yet wire up audio (no `SoundManager` created); that step
  is left for a dedicated audio-integration issue.
- The `Copy()` method re-uses the same `SoundManager*` and `ResourceManager*`
  pointers stored at construction — callers must ensure those outlive all copies.

## Skills / CLAUDE.md instructions applied

- `src/CLAUDE.md`: one class per `.h`/`.cpp` pair, Google C++ style, include root
  is `src/`.
- `src/game/CLAUDE.md`: `OnAddedToScene` / `OnRemovedFromScene` pattern,
  `GameObject` lifecycle invariants.
- `src/audio/CLAUDE.md`: always load through `ResourceManager`, never directly
  via `Sound::GetOrLoad()`; `VirtualSoundInstance*` invalidation rules.
