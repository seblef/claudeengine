# Scene Graph — GameObject Parent/Child Hierarchy

**Issue:** #655  
**Branch:** `feat/scene-graph-gameobject-hierarchy-655`  
**Date:** 2026-06-20

---

## Summary

Introduced parent-child hierarchy into `GameObject` as the foundational runtime layer for the scene graph. The change adds a local transform, parent/child links, and recursive world-transform propagation to all descendants, while preserving existing editor and renderer behaviour.

---

## Changes

### `src/game/GameObject.h`

- Added `#include <vector>`.
- New private members: `local_transform_` (`Mat4f`), `parent_` (`GameObject*`, nullable non-owning), `children_` (`std::vector<GameObject*>`, non-owning).
- New public API:
  - `SetLocalTransform(const Mat4f&)` — primary mutator; propagates recursively.
  - `GetLocalTransform() const`
  - `GetParent() const`
  - `GetChildren() const`
  - `AddChild(GameObject*)` — links parent/child, recomputes local transform.
  - `RemoveChild(GameObject*)` — unlinks; child retains world position.
  - `Reparent(GameObject*)` — convenience wrapper.
- `SetWorldTransform` updated: derives `local_transform_` from parent when present, then delegates to `SetLocalTransform`.
- New `protected` method: `SetWorldTransformPhysics(const Mat4f&)` — physics-only path that propagates downward without back-propagating to parent.
- New `private` method: `UpdateWorldTransform()` — recursive propagation helper called on children.

### `src/game/GameObject.cpp`

- Constructor initialises `local_transform_` to identity and `parent_` to `nullptr`.
- Full implementation of all new methods.
- `#include <algorithm>` added for `std::find` in `RemoveChild`.

### `src/game/GameMesh.cpp`

- `OnBodyTransformUpdated` now calls `SetWorldTransformPhysics` instead of `SetWorldTransform`, preventing physics simulation results from back-propagating up through the parent chain.

### `tests/game/GameObjectHierarchyTest.cpp` (new)

- 13 unit tests covering: no-parent equivalence, propagation to descendants, `AddChild`/`RemoveChild`/`Reparent` world-position preservation, `SetWorldTransform` local-derivation with a parent, physics no-back-propagation, and physics downward propagation.

### `tests/game/CMakeLists.txt` (new)

- Compiles `GameObject.cpp` directly (without the full `game` library) and links against `core` + GTest. This avoids pulling in renderer/physics/audio platform dependencies in the test binary.

### `tests/CMakeLists.txt`

- Added `add_subdirectory(game)`.

---

## Decisions and Rationale

### `SetWorldTransformPhysics` is `protected`, not `private`

The issue calls it "private" in the sense of "not part of the public API", but since `OnBodyTransformUpdated` lives in `GameMesh` (a subclass of `GameObject`), the method must be `protected` to be accessible. Making it `private` would require a `friend` declaration, which would be less clean.

### `world_bbox_` is own geometry only

No aggregation of descendants is performed. Each node's `world_bbox_` reflects only its own local bbox transformed to world space. This matches the issue spec and keeps the bbox update O(1) per node.

### `UpdateWorldTransform()` called on children, not `SetLocalTransform`

After a parent transform changes, children must recompute their world transform without changing their `local_transform_`. `UpdateWorldTransform()` does exactly this: it recomputes `world_transform_` from the (unchanged) `local_transform_` and the (newly updated) parent `world_transform_`, then recurses. Calling `SetLocalTransform` on children would incorrectly overwrite their local transforms.

### Test binary avoids heavy link dependencies

The `game` library links against `renderer`, `physics`, `audio`, `terrain`, etc. — all with platform-specific backends. By compiling `GameObject.cpp` directly in the test target (which only depends on `core`), the hierarchy tests run in any environment without OpenGL or Jolt.

---

## Output / Memory for Next Features

- `GameObject` now has full parent/child support. A future `GamePivot` node (pivot-only, no geometry) can extend `GameObject` with an empty `local_bbox_` — its `world_bbox_` will be degenerate (empty), matching the spec.
- `GameSystem` does not yet expose hierarchy-aware APIs (e.g. add a hierarchy to the scene, serialise parent links to `.map.yaml`). Those are follow-on issues.
- `RemoveChild` silently no-ops if the child is not found. This is intentional — callers should not need to guard against double-removes.
- The `SetWorldTransformPhysics` contract: only `IPhysicsBodyListener::OnBodyTransformUpdated` may call it. Adding other callers would bypass the physics-isolation invariant.

---

## Skills and CLAUDE.md Instructions Used

- **Skill:** `impl-issue`
- **CLAUDE.md rules applied:**
  - One class per `.h` / `.cpp` pair.
  - `cppcheck-suppress unusedStructMember` on all new private members.
  - Include root is `src/`; all `#include` paths are project-relative.
  - Google C++ style guide (enforced via `cpplint`).
  - History `.md` file required for every contribution.
  - Branch prefix `feat/`, PR targets `dev`.
