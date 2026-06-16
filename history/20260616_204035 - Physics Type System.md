# Physics Type System (Issue #560)

## Changes

Eight Jolt-free public headers added to `src/physics/`:

| File | Purpose |
|---|---|
| `MotionType.h` | `enum class MotionType { Static, Kinematic, Dynamic }` |
| `PhysicsShapeType.h` | `enum class PhysicsShapeType` for all primitive and mesh-based shapes |
| `PhysicsShapeDesc.h` | POD struct with a union of per-shape params + static factory helpers |
| `PhysicsMaterialDesc.h` | Surface and mass properties (friction, restitution, mass, damping, gravity factor) |
| `CollisionLayer.h` | `uint16_t` named constants + `kLayerCount` sentinel |
| `PhysicsBodyDesc.h` | Aggregates shape, material, motion type, and collision layer/mask |
| `RaycastResult.h` | Hit result: position, normal, distance, opaque `PhysicsBody*` |
| `IPhysicsBodyListener.h` | Pure-virtual observer receiving `core::Mat4f` transform updates |

## Decisions

**Union in `PhysicsShapeDesc`**: Keeps the struct small and allocation-free. Mesh-based shapes (Terrain, ConvexHull, Exact) carry no extra params because geometry is supplied separately at body-creation time via `MeshData` / `TerrainData`. Factory helpers (`MakeBox`, `MakeSphere`, …) hide union initialisation from callers.

**Forward-declaration of `PhysicsBody` in `RaycastResult.h`**: The full type will be defined in the downstream `PhysicsBody` issue. A pointer member only needs the forward declaration.

**Broad-phase / object-layer filters stay out of public headers**: All Jolt `BroadPhaseLayerInterface` and `ObjectLayerPairFilter` implementations belong in `PhysicsSystem.cpp`. Collision layers are exposed as plain `uint16_t` constants.

**`cppcheck-suppress unusedStructMember`**: Pre-commit hook runs cppcheck; public POD members flagged as unused need suppressions because they have no in-tree consumers yet (this is expected at this skeleton stage).

## Things to keep in mind for next features

- `PhysicsBody.h` (downstream) needs to be consistent with the `PhysicsBody*` forward declaration used in `RaycastResult.h`.
- `PhysicsSystem` must wire the `kLayerCount` constant into Jolt's broad-phase and object-layer interfaces privately.
- All new `.cpp` files in `physics/` must be added to `src/physics/CMakeLists.txt`'s `add_library` source list.
- The `collision_mask` field in `PhysicsBodyDesc` is a raw bitmask — a future helper or `enum class` flags type may improve ergonomics, but keep it simple until needed.

## Skills and CLAUDE.md instructions used

- `impl-issue` skill
- `src/CLAUDE.md`: one class per `.h`, Google C++ style, include root is `src/`, document exposed symbols
- `src/physics/CLAUDE.md`: no Jolt types in public headers, all `#include <Jolt/...>` in `.cpp` / `internal/`
- Memory: `feedback_cppcheck_suppressions.md` — add `cppcheck-suppress unusedStructMember` to public struct members
