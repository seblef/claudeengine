# GameObject base class

**Date:** 2026-05-12
**Issue:** #125
**PR:** #134
**Branch:** feat/game-object-base-class

## Changes

- `src/game/GameObjectType.h` — `enum class GameObjectType { kMesh, kLight, kCamera }`
- `src/game/GameObject.h` — abstract base class declaration
- `src/game/GameObject.cpp` — implementation
- `src/game/CMakeLists.txt` — replaced `game.cpp` placeholder with `GameObject.cpp`
- `src/game/game.cpp` — deleted (placeholder no longer needed)

## Decisions

**World bbox initialisation:** `world_bbox_` is initialised to `local_bbox_` in the constructor (equivalent to applying the identity transform). This avoids a degenerate zero-size bbox before the first `SetWorldTransform` call.

**Lifecycle methods are public:** `OnAddedToScene` and `OnRemovedFromScene` are `public` pure virtual so `GameSystem` can call them directly without a `friend` declaration. This keeps the design simple and consistent with `OnWorldTransformUpdated`.

**cppcheck suppressions:** Private member variables that are only accessed through getters/setters trigger cppcheck's `unusedStructMember` warning. Added `// cppcheck-suppress unusedStructMember` comments following the same convention used throughout `renderer/`.

## Output to keep in mind

- All game object subclasses (GameLight, GameMesh, GameCamera) must call `GameObject(type, local_bbox)` in their constructor initialiser lists.
- `SetWorldTransform()` must be the only way to update the world transform — never write to `world_transform_` directly from subclasses.
- `OnAddedToScene` / `OnRemovedFromScene` are called by `GameSystem::AddObject` / `GameSystem::RemoveObject`.

## Skills / instructions used

- `src/CLAUDE.md`: one class per `.h`/`.cpp` pair; include root is `src/`
- Root `CLAUDE.md`: git workflow, conventional commits
