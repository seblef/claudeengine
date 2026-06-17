# PhysicsGizmoRenderer

**Issue**: #570  
**Branch**: `feat/physics-gizmo-renderer-570`

## Summary

Implements `editor::EnqueuePhysicsGizmo(const game::GameMesh&)` — a free function that pushes a depth-tested wireframe gizmo for a mesh's physics shape into `renderer::WireframeRenderer`.

## Files created / modified

| File | Change |
|---|---|
| `src/editor/PhysicsGizmoRenderer.h` | New — public API declaration |
| `src/editor/PhysicsGizmoRenderer.cpp` | New — implementation |
| `src/editor/CMakeLists.txt` | Added `PhysicsGizmoRenderer.cpp` to the `editor` static library |
| `src/game/GameMesh.h` | Added `GetPhysicsDesc()` and `GetPhysicsBody()` const accessors |

## Design decisions

### GameMesh accessors
`physics_desc_` and `physics_body_` were private with no public accessors.  
Two `[[nodiscard]] const` getters were added directly in the header (inline, zero overhead):
- `GetPhysicsDesc() -> const std::optional<PhysicsBodyDesc>&`
- `GetPhysicsBody() -> const PhysicsBody*`

This is the minimal change needed; no broader refactor.

### Color convention
Colours are per-motion-type as specified in the issue (green=Static, yellow=Kinematic, cyan=Dynamic), computed by a private `ColorForMotionType()` helper in the anonymous namespace.

### ConvexHull / Exact
`GetDebugEdges()` is called on the live `PhysicsBody`, which is only populated after the body is created (i.e. after `OnAddedToScene()`). If `GetPhysicsBody()` returns `nullptr` (mesh not yet in scene), the call is silently skipped — no crash, no gizmo.

### Terrain
Terrain shapes produce no gizmo; they have their own wireframe toggle in `WireframeRenderer`.

## Dependencies satisfied

- `PushCylinder` / `PushCapsule` already present in `WireframeRenderer` (added in a prior PR).
- CPU-side mesh data in `MeshTemplate` already available (prior PR).

## What to keep in mind for next features

- `EnqueuePhysicsGizmo` must be called each frame between `WireframeRenderer::BeginFrame()` and `WireframeRenderer::Render()`. A natural call site is `EditorViewport` during its per-frame gizmo pass.
- The `PhysicsBody*` returned by `GetPhysicsBody()` is non-owning and valid only while the mesh is in scene.

## Skills / CLAUDE.md rules applied

- `src/CLAUDE.md`: one class per `.h`/`.cpp` pair; utility free function in its own file; include root is `src/`.
- `src/editor/CLAUDE.md`: editor is the leaf of the dependency graph; no singleton for this utility.
