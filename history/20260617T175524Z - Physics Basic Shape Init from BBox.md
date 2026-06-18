# Physics Basic Shape Init from Local BBox (#620)

## Summary

When enabling physics on a mesh or switching to a primitive collision shape (Box, Sphere,
Cylinder, Capsule), the shape parameters are now derived from the mesh's local bounding box
rather than defaulting to a fixed `{0.5, 0.5, 0.5}` / `0.5` value.

## Changes

**`src/editor/PropertiesPanel.cpp`**

- Added a `ShapeDescFromBBox(const core::BBox3&, physics::PhysicsShapeType)` helper in the
  anonymous namespace. It converts a bounding-box's half-extents into the appropriate
  `PhysicsShapeDesc` for each primitive type:
  - **Box**: `half_extents = bbox.GetSize() * 0.5`
  - **Sphere**: `radius = max(half_x, half_y, half_z)` — smallest enclosing sphere
  - **Cylinder** (Y-up): `radius = max(half_x, half_z)`, `half_height = half_y`
  - **Capsule** (Y-up): `radius = max(half_x, half_z)`, `half_height = max(half_y - radius, 0.001)`

- **Enable checkbox**: when toggling physics ON, `new_desc.shape` is initialised from
  `ShapeDescFromBBox(mesh->GetTemplate()->GetLocalBBox(), Box)` instead of a hard-coded
  `PhysicsBodyDesc{}` default.

- **Shape-type combo**: all four primitive cases delegate to `ShapeDescFromBBox`; the
  ConvexHull / Exact branches are unchanged.

## Decisions

- Cylinder and Capsule are treated as **Y-axis-upright**, matching Jolt Physics convention.
- The Capsule cylindrical half-height is clamped to `0.001f` (never zero) to keep Jolt happy
  when the mesh is very thin horizontally relative to its height.
- The Sphere uses the **largest** half-extent so the sphere always contains the whole bbox.

## Skills / instructions used

- `src/CLAUDE.md`, `src/editor/CLAUDE.md`, `src/physics/CLAUDE.md` — dependency rules,
  one-class-per-file, Google style.
- `impl-issue` skill workflow (checkout dev → branch → implement → cpplint → commit → PR).

## Notes for next features

- `MeshTemplate::GetLocalBBox()` returns the **local** (unscaled) bbox. If the object has a
  non-uniform scale in its world transform, the physics shape will not reflect that scale
  (Jolt Physics does not support per-body scale anyway — shapes are immutable once created).
  This is consistent behaviour; document it if users ask.
- The helper `ShapeDescFromBBox` is a free function in the anonymous namespace of
  `PropertiesPanel.cpp`. If shape-init logic is needed elsewhere (e.g. a future bulk-enable
  tool), move it to a standalone `PhysicsShapeUtils.cpp/.h`.
