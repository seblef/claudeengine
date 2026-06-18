# Remove PhysicsGizmoRenderer — Issue #627

## Summary

Deleted `PhysicsGizmoRenderer` and replaced its functionality with `PhysicsSystem::DrawDebug()` (added in #624/#625).

## Changes

| File | Change |
|---|---|
| `src/editor/PhysicsGizmoRenderer.h` | Deleted |
| `src/editor/PhysicsGizmoRenderer.cpp` | Deleted |
| `src/editor/EditorViewport.cpp` | Removed `#include` and the `EnqueuePhysicsGizmo()` call block |
| `src/editor/RenderingSettingsPanel.h` | Removed `physics_gizmos_enabled_` member, getter, and setter |
| `src/editor/RenderingSettingsPanel.cpp` | Removed "Physics shapes" checkbox and its separator |
| `src/editor/CMakeLists.txt` | Removed `PhysicsGizmoRenderer.cpp` from source list |

## Decisions and rationale

**Remove the "Physics shapes" checkbox entirely** — it only gated `EnqueuePhysicsGizmo()`. Since `PhysicsSystem::DrawDebug()` is now the sole driver of physics shape visualization and was already unconditionally called (from PR #625), the checkbox had become dead UI. Repurposing it as a DrawDebug() on/off toggle was considered but left out of scope; the existing "Physics Debug Draw" body-mode selector (Selected Only / All Bodies) provides sufficient granularity.

**No new includes added** — the `game/GameMesh.h`, `game/GameObjectType.h`, and `physics/PhysicsBody.h` headers that remained in `EditorViewport.cpp` are all still consumed by the `DrawDebug()` selected-body filtering block, so no dead includes.

## Key outputs for future reference

- Physics shape visualization (body wireframes) in the editor now uses Jolt's native `DrawBodies()` path, which renders the **actual** convex hull geometry rather than source-mesh triangles.
- The `RenderingSettingsPanel` now only exposes the "Physics Debug Draw" collapsible (body mode, constraints, contact points, broadphase) with no top-level "Physics shapes" toggle.
- PR #634 closed issue #627.

## Skills and CLAUDE.md instructions used

- `impl-issue` skill
- `src/CLAUDE.md`: one class per file, Google C++ style, include root is `src/`
- `src/editor/CLAUDE.md`: editor is the dependency-graph leaf; panel classes must be pure UI
