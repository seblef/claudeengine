# CLAUDE.md — physics module

## Role

The `physics` module wraps [Jolt Physics](https://github.com/jrouwe/JoltPhysics) and exposes a clean, Jolt-free API to the rest of the engine. It is consumed by `game/` and must not depend on `game/`, `renderer/`, `editor/`, or `gldevices/`.

## Dependency graph

```
app → game → physics → core
```

## Module structure

| File(s) | Responsibility |
|---|---|
| `MotionType.h` | `enum class MotionType` — Static / Kinematic / Dynamic |
| `PhysicsShapeType.h` | `enum class PhysicsShapeType` — primitive and mesh shape variants |
| `PhysicsShapeDesc.h` | POD struct with per-shape union + factory helpers; no Jolt dependency |
| `PhysicsMaterialDesc.h` | Surface/mass properties (friction, restitution, mass, damping, gravity) |
| `CollisionLayer.h` | `uint16_t` layer constants + `kLayerCount`; Jolt filters stay in `.cpp` |
| `PhysicsBodyDesc.h` | Aggregates shape, material, motion type, and collision layer/mask |
| `RaycastResult.h` | Raycast hit result; forward-declares `PhysicsBody` |
| `IPhysicsBodyListener.h` | Observer interface for body transform updates |
| `JoltDebugRenderer.h/.cpp` | `JPH::DebugRenderer` bridge; forwards draw calls to `WireframeRenderer` |

## Guidelines

Follow all rules in `src/CLAUDE.md`. Additionally:

- **Jolt types must not appear in any `physics/*.h` included by `game/` or `editor/`.**  
  Keep all `#include <Jolt/...>` in `physics/*.cpp` and private headers (e.g. `physics/internal/`).
- Public headers expose only engine-native types (e.g. `core::Vec3`, plain POD structs, or forward declarations of opaque handles).
- `physics/` must not include any header from `game/`, `renderer/`, `editor/`, or `gldevices/`.  
  **Exception:** `JoltDebugRenderer.cpp` PRIVATE-links against `renderer` to forward draw calls to `WireframeRenderer::PushSegment`. No renderer type appears in any public physics header.
- `JoltDebugRenderer.cpp` must be compiled with `-fno-rtti` (Jolt requires it). This is enforced via `set_source_files_properties` in `CMakeLists.txt`.
- One class per `.h` / `.cpp` pair; utility functions go in `*Utils.cpp`.
