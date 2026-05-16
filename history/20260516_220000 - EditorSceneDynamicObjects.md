# EditorScene: dynamic object management (add/remove/IsDynamic)

**Date:** 2026-05-16  
**Branch:** feat/editor-scene-dynamic-objects  
**Issue:** #217

---

## Summary

`EditorScene` previously owned all its objects as fixed `unique_ptr` fields
(`floor_`, `cube_`, `light_`). This change introduces a dynamic object pool that
allows the editor to create and delete objects at runtime.

---

## Changes

### `EditorScene.h`

- Removed the `cube_` field (`std::unique_ptr<game::GameMesh>`); the cube is
  now the first entry in `dynamic_objects_`.
- Added `std::vector<std::unique_ptr<game::GameObject>> dynamic_objects_` as the
  first private member, ensuring it is destroyed before `floor_` and `light_`
  (reverse RAII destruction order prevents dangling references at shutdown).
- Added three public methods:
  - `AddDynamicObject(unique_ptr<GameObject>)` — transfers ownership, registers
    with `GameSystem`, appends to `objects_`, returns raw pointer.
  - `RemoveDynamicObject(GameObject*)` — no-op for static objects; otherwise
    clears selection if needed, calls `GameSystem::RemoveObject`, removes from
    both `objects_` and `dynamic_objects_`.
  - `IsDynamic(const GameObject*) const` — linear scan over `dynamic_objects_`.
- Updated class-level doc comment to reflect the new design.

### `EditorScene.cpp`

- Added `#include <algorithm>` for `std::find_if`, `std::remove`, `std::any_of`.
- Constructor: cube creation now calls `AddDynamicObject(std::move(cube))`
  instead of assigning to `cube_` and manually calling `GameSystem::AddObject`.
- Destructor: replaced the loop over `objects_` with explicit per-category
  unregistration (`dynamic_objects_` first, then `light_`, then `floor_`),
  keeping symmetry with `AddDynamicObject`/`RemoveDynamicObject` and avoiding
  double-unregister on objects already removed via `RemoveDynamicObject`.
- Implemented `AddDynamicObject`, `RemoveDynamicObject`, `IsDynamic`.

---

## Decisions and rationale

| Decision | Rationale |
|---|---|
| `dynamic_objects_` declared before `floor_`/`light_` | C++ destroys members in reverse declaration order; dynamic objects may hold references to scene-level resources that exist as long as `floor_`/`light_` are alive. |
| Cube moved to dynamic pool | Issue spec: cube must become user-deletable. Keeping it as a fixed field would require special-casing in `IsDynamic`. |
| `objects_` kept in sync (not rebuilt on demand) | `GetObjects()` is called every frame from the UI; rebuilding is wasteful. Linear sync on add/remove is O(n) but n is tiny in an editor scene. |
| Explicit unregister loop in destructor instead of reusing `objects_` | After `RemoveDynamicObject` calls, `objects_` may no longer contain removed entries; looping it would skip them. Explicit loops are safe regardless of prior removals. |

---

## Output to keep in mind

- `IsDynamic` is O(n); for an editor with tens of objects this is fine.
- Callers that iterate `GetObjects()` and call `RemoveDynamicObject` inside the
  loop must **not** use a range-for over the returned vector — obtain indices or
  copy the list first, as `RemoveDynamicObject` mutates `objects_`.
- `floor_` and `light_` are never in `dynamic_objects_`, so `IsDynamic` returns
  `false` for them — they cannot be deleted via `RemoveDynamicObject`.

---

## Skills / instructions referenced

- `src/CLAUDE.md`: one class per file, Google C++ style, include root is `src/`.
- `src/editor/CLAUDE.md`: one class per `.h`/`.cpp` pair, editor is leaf of dependency graph.
- `CLAUDE.md` (root): conventional commits, cpplint, history file required.
