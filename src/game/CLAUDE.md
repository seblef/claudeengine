# CLAUDE.md — game module

## Role

The `game` module is the architectural layer between the renderer and the application entrypoint. It owns the game loop, manages game object lifecycles, and bridges game-level concepts (world transforms, scene membership) to renderer-level concepts (renderables, lights, camera).

## Dependency graph

```
app → game → renderer → abstract → core
```

The `game` module must not be included by `renderer`, `abstract`, or `core`.

## Module structure

| File(s) | Responsibility |
|---|---|
| `GameSystem` | Singleton owning the game loop: event pump, time tracking, camera controller dispatch, object lifecycle, renderer call |
| `GameObject` | Abstract base for all game objects; manages world transform and bbox |
| `GameObjectType` | Enum discriminating mesh / light / camera objects |
| `GameLight` / `GameLightDesc` | Wraps any `renderer::Light` subtype; creates and owns the renderer light |
| `GameMesh` | Wraps a `MeshTemplate` and a `RenderableMeshInstance`; instance exists only while in scene |
| `MeshTemplate` | Reference-counted resource (`core::Resource`) loading a `RenderableMesh` from disk; ensures each file is loaded once |
| `GameCamera` | Wraps `core::Camera`; derives position/orientation from world transform |
| `ICameraController` | Abstract camera controller interface |
| `FPSCameraController` | FPS-style controller: mouse look, arrow keys for movement, A=up, Z=down |
| `GameObjectVisitor` | Abstract visitor for all concrete GameObject subclasses; implement `Visit(GameMesh&)`, `Visit(GameLight&)`, `Visit(GameCamera&)` to dispatch per-type logic without casting |
| `MapLoader` | Static utility; `MapLoader::Load(path, video)` parses a `.map.yaml` file and returns a `MapData` (name, map_size, global_light, objects); objects are NOT added to GameSystem — the caller does that |
| `GameSoundEmitter` | `GameObject` subclass that loops a 3D positional sound at its world position; registers with `SoundManager` on scene entry |
| `SoundEffectComponent` | Lightweight component (not a `GameObject`) that fires one-shot sounds; embed in any entity and call `Trigger(worldPos)` |

## Key invariants

- `GameObject::SetWorldTransform()` is the single entry point for spatial updates; it recomputes the world bbox and calls `OnWorldTransformUpdated()`.
- `OnAddedToScene()` / `OnRemovedFromScene()` register and unregister renderer objects; a `RenderableMeshInstance` only exists while the `GameMesh` is in the scene.
- `MeshTemplate` is keyed by mesh file path; call `MeshTemplate::GetOrLoad()` — never `new MeshTemplate()` directly.
- `GameSystem` is the sole consumer of the `EventManager` queue; it routes events to the active camera controller before calling `controller->Update(dt)`.

## Scene objects vs components

`GameObject` subclasses are **scene objects**: they have persistent identity, live in the `GameSystem` object list, and receive `OnAddedToScene` / `OnRemovedFromScene` / `OnWorldTransformUpdated` callbacks. Add a new `GameObject` subclass when the entity has independent scene presence (is placed in a map, is selectable in the editor, owns renderer resources).

Plain classes (like `SoundEffectComponent`) are **components**: they are embedded as members in an owning entity and have no scene lifecycle of their own. Use a component when the behaviour is transient or when multiple instances need to coexist on a single entity. Components call into `SoundManager`, `ParticleRenderer`, etc. directly — they do not go through `GameSystem`.

When in doubt: if it appears in a `.map.yaml`, it is a scene object. If it is spawned at runtime by game logic, it is a component.

## Guidelines

Follow all rules in `src/CLAUDE.md`. Additionally:
- One class per `.h` / `.cpp` pair.
- Structs used purely as data holders (e.g. `GameLightDesc`) may be header-only.
- Do not include any platform or OpenGL headers directly; go through `abstract/`.
