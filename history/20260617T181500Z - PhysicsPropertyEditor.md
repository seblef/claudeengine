# Physics Property Editor — Issue #572

## Summary

Added a collapsible **Physics** section to the GameMesh properties panel, letting users configure physics bodies without editing YAML manually.

## Changes

### `src/physics/PhysicsMaterialDesc.h` / `PhysicsShapeDesc.h` / `PhysicsBodyDesc.h`
Added `operator==` / `operator!=` to all three POD structs so the editor can detect whether a physics description actually changed before pushing an undo command. `PhysicsShapeDesc::operator==` dispatches on the active union member to avoid comparing padding bytes.

### `src/game/GameMesh.h` / `GameMesh.cpp`
Added `ClearPhysicsDesc()`: resets `physics_desc_` to `std::nullopt` and destroys any live physics body if the mesh is already in scene. Complements `SetPhysicsDesc()` for the enable/disable checkbox.

### `src/editor/commands/PhysicsPropertyCommand.h` / `.cpp`
New undoable command. Stores `std::optional<PhysicsBodyDesc>` for before and after, so enabling, disabling, and property edits are all reversible through the same code path.

### `src/editor/PropertiesPanel.h` / `.cpp`
- `RenderMeshProperties` made non-static and takes `GameMesh*` (non-const) since it now calls `SetPhysicsDesc` / `ClearPhysicsDesc`.
- Added `before_physics_desc_` member (`std::optional<PhysicsBodyDesc>`) for drag-based undo capture.
- Implemented the Physics collapsible section:
  - **Enable physics** checkbox: immediately enables (default `PhysicsBodyDesc`) or clears, pushes `PhysicsPropertyCommand`.
  - **Shape type** combo: Box / Sphere / Cylinder / Capsule / ConvexHull / Exact. Terrain excluded since `GameMesh` logs a warning and redirects to `GameTerrain`.
  - Per-shape parameters shown/hidden conditionally: `Half extents` (Box only), `Radius` (Sphere / Cylinder / Capsule), `Half height` (Cylinder / Capsule). ConvexHull and Exact show an `ImGui::TextDisabled` label.
  - **Motion type** combo: Dynamic and Kinematic disabled (`BeginDisabled`) when shape is Exact or Terrain.
  - **Material sliders**: Friction, Restitution (0–1), Lin. damping, Ang. damping (0–1), Gravity factor (0–4).
  - **Mass** as `DragFloat`.
  - **Collision layer** as `InputInt` clamped to `[0, kLayerCount)`.
  - **Collision mask** as `InputScalar` with `ImGuiDataType_U16` and `"%04X"` format.

## Key Decisions

| Decision | Rationale |
|---|---|
| `operator==` on physics POD structs | Required by the `track()` lambda to detect real changes and avoid redundant undo entries. Implemented in the struct header since equality is a natural property of a value type. |
| `ClearPhysicsDesc()` on `GameMesh` | "Remove physics" requires a distinct setter (not an overload of `SetPhysicsDesc`) to express intent clearly and to avoid passing a sentinel value. |
| `PhysicsPropertyCommand` with `optional<PhysicsBodyDesc>` | A single command type covers enable, disable, and property edits; `nullopt` naturally represents the "no physics" state. |
| `std::optional<PhysicsBodyDesc> before_physics_desc_` in panel | Follows the same `IsItemActivated` / `IsItemDeactivatedAfterEdit` pattern as `before_snapshot_` for lights and `before_transform_` for transforms. |
| Terrain excluded from shape combo | `GameMesh::CreatePhysicsBody()` logs a warning and skips creation for Terrain; exposing it in the editor would be misleading. |
| No separate Tool class | Consistent with the issue specification and the editor CLAUDE.md note that panel-owns-logic is acceptable when there is no per-frame algorithm. |

## Points to Keep in Mind for Future Features

- The `track()` lambda inside `RenderMeshProperties` captures `before_physics_desc_` from the enclosing `PropertiesPanel` instance. If another editor panel ever reuses that member, consider renaming or scoping the before-capture differently.
- `kLayerCount = 4` is the hard-coded layer ceiling used to clamp the collision layer input. If more layers are added, the clamp is automatically correct.
- `ClearPhysicsDesc()` is the only entry point for disabling physics from the outside. `GameMesh` has no "null desc" sentinel in `SetPhysicsDesc` — keep it that way to keep the API clear.

## Skills Used

- `impl-issue`

## CLAUDE.md Guidelines Especially Applied

- *One class per .h / .cpp pair* — `PhysicsPropertyCommand` has its own file pair.
- *Include root is `src/`* — all includes use project-relative paths.
- *Google C++ style guide* — cpplint ran clean.
- *Panel classes must be pure UI* — all logic (desc construction, `SetPhysicsDesc` calls) lives in `RenderMeshProperties` and a helper lambda; no separate tool class needed.
- *Jolt types must not appear in `physics/*.h` included by `game/` or `editor/`* — the new equality operators touch only engine-native types.
