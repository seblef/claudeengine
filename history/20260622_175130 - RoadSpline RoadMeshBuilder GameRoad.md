# RoadSpline, RoadMeshBuilder, GameRoad — issue #716

## What was added

### New module `track/`

- **`track/RoadSpline`**: Looping Catmull-Rom spline. Stores `Vec3f` control points, evaluates position (`Sample(t)`) and tangent (`GetTangent(t)`) at uniform parameter `t ∈ [0, 1)`. Arc length approximated via 200 chord samples. No dependency on `game/` or `renderer/`.

- **`track/RoadMeshBuilder`**: Static factory that builds a `RoadMeshData` (interleaved `float` vertices + `uint32_t` indices) ribbon along a spline. Cross-section count derived from `max(8, round(length × samples_per_metre))`. Vertices are draped onto a terrain height function (with +0.02 m z-fight offset); the height function is `nullptr`-safe (flat road). Left/right edges close into a loop at the last segment.

### `game/GameRoad`

Mirrors `GameMesh`/`GameTerrain` lifecycle. On `RegenerateMesh()`:
1. Calls `RoadMeshBuilder::Build()`.
2. Converts raw interleaved floats → `core::Vertex3D` (fills normal/binormal/tangent from the spline tangent at each cross-section).
3. Uploads to a new `renderer::GeometryData` → `renderer::Mesh` → `renderer::MeshInstance`.
4. Destroys and recreates a static `PhysicsBody` via `PhysicsSystem::CreateBodyWithMesh()` with an `Exact` shape.

Material is stored as a `GameMaterial*` (non-owning, caller manages lifetime), matching the game-layer convention.

### `GameObjectType` / `GameObjectVisitor`

- Added `kRoad` to `GameObjectType`.
- Added `virtual void Visit(GameRoad&)` to `GameObjectVisitor`.
- All existing visitor implementations (`PlayModeManager::EnterVisitor`, `MapSerializer::SerializeVisitor`) get no-op or real overrides.

### MapLoader / MapSerializer

YAML format accepted:
```yaml
- type: road
  name: MainLoop
  width: 8.0
  samples_per_metre: 1.0
  material: materials/road_asphalt.mat.yaml
  control_points:
    - [10, 0, 0]
    - [50, 0, 20]
    - [50, 0, -20]
    - [10, 0, -40]
```

`MapLoader` adds a third pass after parent-linking: finds the `GameTerrain` in the loaded objects (if any), builds a `height_fn` closure over its `TerrainData`, then calls `RegenerateMesh()` on every `GameRoad`.

`MapSerializer` emits the road block from `GameRoad::GetSpline()`, `GetWidth()`, `GetSamplesPerMetre()`, and `GetMaterialPtr()`.

## Key decisions

- **`track/` has no `game/` dependency.** `RoadSpline` and `RoadMeshBuilder` use only `core/` types, keeping the dependency graph clean: `track → core`. The `game/` layer bridges track utilities to the renderer and physics.

- **`RoadMeshData` uses raw `float` / `uint32_t` vectors** rather than `core::Vertex3D` because `RoadMeshBuilder` does not depend on the renderer vertex format. The conversion happens in `GameRoad::RegenerateMesh()`.

- **Looping strip** — the last cross-section's right vertex connects back to cross-section 0, making the road a seamless loop matching the looping Catmull-Rom spline.

- **Physics shape is `Exact`** (triangle mesh), which is correct for a static driveable surface. `PhysicsSystem::CreateBodyWithMesh()` caches the Jolt shape by pointer key; we pass `nullptr` as cache key because each road has unique geometry rebuilt on every call.

- **`RegenerateMesh` is deferred** in `MapLoader`: all objects are parsed first, then a single pass finds the terrain and regenerates roads. This means road meshes are always draped correctly even if the terrain appears after the roads in the YAML.

## Output to keep in mind

- A future editor panel should call `RegenerateMesh()` interactively whenever the user moves a control point or changes `width`/`samples_per_metre`. The road's GPU mesh and physics body are entirely rebuilt on each call, which is acceptable for editing cadence.
- Arc-length re-parameterisation (uniform spacing independent of control-point density) is not implemented — the spec explicitly defers it. Uneven control-point spacing will produce non-uniform mesh density.
- The `GameRoad` does not currently register with the editor selection / gizmo system. A follow-up issue should handle that.

## Skills and CLAUDE.md sections used

- `impl-issue` skill
- `src/CLAUDE.md`: one class per file, Google C++ style, `#include` root is `src/`
- `src/game/CLAUDE.md`: scene object lifecycle, `OnAddedToScene`/`OnRemovedFromScene`, `GameObjectVisitor` extension pattern
- `src/physics/CLAUDE.md`: Jolt types must not leak into public headers
