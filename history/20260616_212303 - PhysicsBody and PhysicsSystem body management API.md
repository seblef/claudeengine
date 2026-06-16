# PhysicsBody and PhysicsSystem body management API

**Issue:** #563  
**Branch:** `feat/physics-body-management-563`

---

## What was implemented

### `PhysicsBody` (`src/physics/PhysicsBody.h/.cpp`)

Full implementation of the `PhysicsBody` class, replacing the prior stub:

- **`GetWorldTransform()`** — reads the current world-space matrix from Jolt via `BodyInterface::GetWorldTransform`.
- **`SetWorldTransform()`** — pushes a new transform:
  - Static bodies: `SetPositionAndRotation` with `DontActivate`.
  - Kinematic bodies: `MoveKinematic` with a nominal 60 Hz dt (see _Decisions_ below).
  - Dynamic bodies: asserts in debug builds (illegal call).
- **`ApplyForce()` / `ApplyImpulse()` / `SetLinearVelocity()`** — delegate to Jolt's `BodyInterface`; each asserts `motion_type_ == Dynamic` in debug.
- **`GetDebugEdges()`** — returns cached wireframe edges (populated for ConvexHull / Exact shapes; empty for primitives).

Private constructor is `friend` with `PhysicsSystem`; all instances are created through the factory methods.

### `PhysicsSystem` additions (`src/physics/PhysicsSystem.h/.cpp`)

- **`CreateBody()`** — primitive shapes (Box, Sphere, Cylinder, Capsule). Logs FATAL for unsupported shape types.
- **`CreateBodyWithMesh()`** — ConvexHull (`JPH::ConvexHullShapeSettings`) or Exact (`JPH::MeshShapeSettings`). Logs FATAL if Exact + non-Static. Populates `debug_edges_` from the triangle index buffer.
- **`CreateTerrainBody()`** — converts `terrain::TerrainData` uint16 heightmap to float world-space heights, builds `JPH::HeightFieldShapeSettings`. Asserts square terrain. Always Static / kLayerWorld / no listener.
- **`DestroyBody()`** — calls `RemoveBody` + `DestroyBody` on the Jolt interface then removes from `bodies_`.
- **`bodies_`** changed from `std::vector<PhysicsBody*>` (non-owning) to `std::vector<std::unique_ptr<PhysicsBody>>` (owning). Bodies are automatically destroyed when `PhysicsSystem` is torn down.
- `RegisterBody` / `UnregisterBody` removed (superseded by `CreateBody` / `DestroyBody`).

### `PhysicsShapeDesc.h`

Added an explicit default constructor to resolve a pre-existing latent compile error: the anonymous union contained a `Box` member whose `Vec3f half_extents` field has default member-initializers, making `Box`'s default constructor non-trivial and the union's (and therefore `PhysicsShapeDesc`'s) default constructor implicitly deleted. The issue was never triggered before because no translation unit previously included `PhysicsBodyDesc.h` transitively inside `PhysicsSystem.cpp`.

### `CMakeLists.txt` (`src/physics/`)

- Added `PhysicsBody.cpp` to the library sources.
- Added `terrain` as a private link dependency (required for `CreateTerrainBody`).

---

## Decisions and rationale

### Jolt types in `PhysicsBody.h`
The CLAUDE.md rule forbids Jolt types in public headers. `JPH::BodyID` is internally a `uint32_t` wrapper, so `body_id_` is stored as `uint32_t` (matching the prior `jolt_id_` pattern). `JPH::PhysicsSystem*` is only forward-declared — no Jolt header is pulled in.

### `SetWorldTransform` — kinematic dt
`BodyInterface::MoveKinematic` requires a `float inDeltaTime` to compute the body's velocity (so kinematic bodies can correctly push dynamic ones). Since `SetWorldTransform` has no dt parameter (matching the issue spec), a nominal dt of `1/60 s` is used. This works correctly at 60 Hz; a future API can expose an explicit dt overload if variable-rate kinematic movement is needed.

### Mass override
`PhysicsBodyDesc::material.mass` is forwarded to Jolt via `EOverrideMassProperties::CalculateInertia`, letting the physics engine derive inertia from shape geometry while honouring the game-specified mass.

### Terrain
`TerrainData` stores heights as `uint16_t`. The conversion `min_h + (sample / 65535.0f) * range` maps to world-space metres; `HeightFieldShapeSettings` receives `scale.y = 1.0f` so sample values are already in the correct unit. Terrain must be square (`w == h`); a non-square terrain triggers `LOG_F(FATAL)`.

### Debug edges
For ConvexHull and Exact shapes, `BuildDebugEdges` extracts unique edges from the triangle index buffer (deduplicates via a `std::set` of canonical sorted pairs) and stores them as consecutive Vec3f pairs — the format expected by `WireframeRenderer`.

---

## Output to keep in mind for future work

- `SetWorldTransform` for kinematic bodies uses a hardcoded 60 Hz dt. If the game needs variable-rate kinematic movement with correct collision response, add a `void MoveKinematic(const core::Mat4f&, float dt)` overload or store a pending transform in `PhysicsBody` and apply it in `Step()`.
- `CreateTerrainBody` requires a square heightmap. Rectangular terrain support would require cropping or padding to a power-of-two square, which Jolt recommends for efficiency anyway.
- `CreateBody` does not handle the `Terrain` or `ConvexHull` / `Exact` shape types — those require `CreateBodyWithMesh` / `CreateTerrainBody`. Calling `CreateBody` with those types logs FATAL.
- The `terrain` module is now a private dependency of `physics`. This is intentional (issue requirement) and does not violate the dependency rules.

---

## Skills and CLAUDE.md instructions used

- `impl-issue` skill
- `src/physics/CLAUDE.md` — Jolt types must not appear in public headers; one class per `.h/.cpp`; Google C++ style guide.
- `src/CLAUDE.md` — include root is `src/`; Google C++ style guide; one class per file.
- Root `CLAUDE.md` — conventional commits; history file requirement; cpplint before commit.
