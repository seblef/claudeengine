# feat(game): virtual Update(float dt) on GameObject — issue #710

## Changes

### `src/game/GameObject.h`
Added public virtual method:
```cpp
virtual void Update(float dt);
```
Documented as a per-frame tick called by `GameSystem` for root objects, propagating recursively to children. Subclasses that override must call `GameObject::Update(dt)` at the end.

### `src/game/GameObject.cpp`
Added implementation:
```cpp
void GameObject::Update(float dt) {
    for (GameObject* child : children_)
        child->Update(dt);
}
```
Default is a pure propagation — leaf nodes are no-ops, non-leaf nodes recurse.

### `src/game/GameSystem.cpp`
Added root-object tick loop after the camera controller update and before the physics step:
```cpp
for (GameObject* obj : objects_) {
    if (obj->GetParent() == nullptr)
        obj->Update(dt);
}
```
Only root objects are ticked here to avoid double-ticking children already reached by the recursive propagation.

## Decisions and rationale

- **Placement in `GameSystem::Update()`**: Inserted after `camera_controller_->Update(dt)` and before `PhysicsSystem::Step(dt)`. This lets future game-logic overrides (e.g. `GameVehicle` reading input and applying forces) execute before the physics simulation consumes them.
- **Root-only tick**: Ticking only objects with no parent is the standard scene-graph pattern; recursive propagation handles the rest. This prevents children that are also in `objects_` from being ticked twice.
- **No impact on existing objects**: All current `GameObject` subclasses (`GameMesh`, `GameLight`, `GameCamera`, `GameTerrain`, `GameSoundEmitter`, `GamePlayerStart`, `GameParticleSystem`) inherit the default no-op-for-leaves / propagate-for-parents behaviour. `GameParticleSystem` still has its own separate `Update(elapsed_time_, dt)` call with two arguments — a different signature, not affected.

## Output to keep in mind

- `GameVehicle` (next M2 issue) will be the first subclass to provide a real `Update(float dt)` override. It must call `GameObject::Update(dt)` at the end.
- The existing `GameParticleSystem` loop in `GameSystem::Update()` uses a two-argument signature and is unrelated to the new single-argument virtual. When `GameParticleSystem` is eventually refactored, consider folding its tick into the new virtual mechanism.

## Skills and instructions used

- `impl-issue` skill
- `src/CLAUDE.md`: Google C++ style guide, one class per file, include paths from `src/`
- `src/game/CLAUDE.md`: scene-object lifecycle conventions, `GameSystem` architecture
- Root `CLAUDE.md`: git workflow (branch from `dev`, conventional commits, cpplint, PR to `dev`)
