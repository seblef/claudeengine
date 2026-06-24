# feat(physics): infer wheel geometry from mesh and support convex-hull vehicle body

## Changes

### `src/physics/VehicleDesc.h`
- Added `WheelGeometry { float radius; float width; }` struct.
- Removed `radius` and `width` from `WheelDesc`; geometry now comes exclusively from `WheelGeometry` inferred from the wheel mesh bounding box.

### `src/physics/PhysicsSystem.h` / `PhysicsSystem.cpp`
- `CreateVehicle` extended with `front_wheel_geo`, `rear_wheel_geo`, and optional `body_vertices`/`body_vertex_count` params.
- `BuildWheelSettings` (file-local) now receives a `WheelGeometry` for `mRadius`/`mWidth` instead of reading them from `WheelDesc`.
- When `body_vertices != nullptr`, a `ConvexHullShape` is built from the provided `core::Vec3f*` points; otherwise a `BoxShape` from `desc.half_extents` is used (backward-compatible default). The COM-offset `OffsetCenterOfMassShape` wrapping applies to both paths.

### `src/game/VehicleTemplate.h` / `VehicleTemplate.cpp`
- Added `front_wheel_geo_`, `rear_wheel_geo_` (`physics::WheelGeometry`) and `use_convex_hull_body_` (`bool`) members with accessors.
- `InferWheelGeometry()` helper (file-local): reads the mesh bounding box after load, derives `radius = bbox.GetSize().y * 0.5f` and `width = bbox.GetSize().x`. Emits `LOG_F(WARNING, ...)` when the Y and Z extents differ by more than 10 % of the Y extent (non-circular cross-section).
- `ParseWheelDesc()` and the new-format `read_wheel` lambda no longer read `radius` or `width` from YAML.
- `body_shape` YAML field parsed under `physics:` section (default `"box"`).

### `src/game/GameVehicle.cpp`
- `Activate()` fetches geometry and optional body vertices from the template and forwards them to `CreateVehicle`.

### `src/editor/VehicleEditorWindow.h` / `VehicleEditorWindow.cpp`
- Added `use_convex_hull_body_` member, reset in `Open()`.
- `LoadFromYaml`: reads `body_shape` from `physics:` node.
- `SaveToYaml`: writes `body_shape: convex_hull` or `body_shape: box`.
- `DrawPhysicsSection`: added `ImGui::Checkbox("Convex hull body", ...)` so round-tripping through the editor preserves the setting.

### `data/vehicles/hell.vehicle.yaml`
- Removed `radius` / `width` from all four wheel nodes.
- Added `body_shape: convex_hull`.

### `data/vehicles/test_box.vehicle.yaml`
- Removed `radius` / `width` from all four wheel nodes.
- Added `body_shape: box`.

## Decisions

### Axis convention
Wheel meshes are authored with their axle along **X**:
- `radius = bbox.GetSize().y * 0.5f`
- `width  = bbox.GetSize().x`

A warning is emitted when Y and Z extents differ by > 10 % of Y (non-circular cross-section → authoring mistake).

### Convex hull path
Mirrors `CreateBodyWithMesh`'s `ConvexHullShape` path. The body vertices are already available via `MeshTemplate::GetCPUPositions()`. Passing `nullptr` falls back to the box shape, keeping all existing code paths unaffected.

### Editor round-trip
The `body_shape` key is written by `SaveToYaml` so a round-trip through the editor never silently resets a `convex_hull` vehicle to `box`.

## Notes for next features

- `WheelDesc` no longer carries geometry; `WheelGeometry` is the authoritative source.
- Front and rear axle geometries are stored separately in `VehicleTemplate` because different axle meshes could differ.
- `GetCPUPositions()` may be empty for procedural templates; `GameVehicle::Activate()` checks `UseConvexHullBody()` before accessing the vector to avoid a null dereference.

## Skills / instructions followed

- `src/CLAUDE.md` — one class per file, Google C++ style, include root = `src/`
- `src/physics/CLAUDE.md` — no Jolt types in public headers
- `src/game/CLAUDE.md` — game module dependency rules
- `src/editor/CLAUDE.md` — editor is leaf, pure UI in panel classes
