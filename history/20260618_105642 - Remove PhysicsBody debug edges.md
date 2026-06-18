# Remove PhysicsBody Debug Edges (#628)

## Summary

Removed `BuildDebugEdges`, `debug_edges_`, and `GetDebugEdges` from the physics module.
These were exclusively used by `PhysicsGizmoRenderer`, which was deleted in #627.

## Changes

### `src/physics/PhysicsBody.h`
- Removed `#include <vector>` (no longer needed after removing the field)
- Removed `GetDebugEdges()` public accessor
- Removed `debug_edges_` private field

### `src/physics/PhysicsSystem.cpp`
- Removed `#include <set>` (only used by `BuildDebugEdges`)
- Removed the `BuildDebugEdges` free function (~25 lines, O(N·log N) per mesh)
- Removed the `body->debug_edges_ = BuildDebugEdges(...)` call at body creation time

## Decisions

- The `<vector>` include was also dropped from `PhysicsBody.h` since no public or private
  member uses `std::vector` anymore. This keeps the public header lean.
- `<set>` was removed from `PhysicsSystem.cpp` because it was only pulled in for the
  `std::set<std::pair<…>>` deduplication inside `BuildDebugEdges`.

## Impact

- Body creation for `ConvexHull` and `Exact` mesh shapes no longer iterates every triangle
  to build a wireframe cache, eliminating visible lag on large meshes.
- `PhysicsBody` is smaller (no heap-allocated vector) and has one fewer public method.

## Skills / CLAUDE.md rules applied

- `impl-issue` skill workflow: checkout dev → branch → implement → cpplint → commit → PR
- `src/CLAUDE.md`: one class per `.h`/`.cpp`, Google C++ style, include root is `src/`
- `src/physics/CLAUDE.md`: no Jolt types in public headers; physics must not depend on editor/renderer
