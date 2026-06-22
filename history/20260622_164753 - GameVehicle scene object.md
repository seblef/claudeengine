# GameVehicle Scene Object (Issue #713)

## Overview

Part of **M2 — Driveable Car**. Implements `GameVehicle`, a `GameObject` subclass for a wheeled vehicle placed in the map. In editor mode the body and wheel meshes are visible but physics is inactive. `PlayModeManager::Enter()` activates the simulation and wires a `PlayerVehicleController`.

## New files

| File | Role |
|---|---|
| `src/game/GameVehicle.h` | Class declaration: inherits `GameObject` + `IPhysicsBodyListener` |
| `src/game/GameVehicle.cpp` | `Create()` factory, physics activation, per-frame update, body listener |

## Modified files

| File | Change |
|---|---|
| `src/game/GameObjectType.h` | Added `kVehicle` to the enum |
| `src/game/GameObjectVisitor.h` | Forward-declared `GameVehicle`; added pure virtual `Visit(GameVehicle&)` |
| `src/game/CMakeLists.txt` | Added `GameVehicle.cpp` to the game static library |
| `src/game/MapLoader.cpp` | Added `ParseVehicle()` helper and `"vehicle"` type dispatch |
| `src/editor/MapSerializer.h/.cpp` | Added `Visit(GameVehicle&)` — emits `type: vehicle` with `desc` + `transform` |
| `src/editor/PlayModeManager.h/.cpp` | `EnterVisitor` finds first vehicle; `Enter()` creates `PlayerVehicleController`, assigns it, calls `Activate()`; `Tick()` drives `vehicle->Update(dt)`; `Exit()` resets controller |

## Design decisions

### Vehicle descriptor YAML (`*.vehicle.yaml`)
The `Create(desc_path, video)` factory reads a single YAML file that encodes **both** the `VehicleDesc` physics properties and the mesh paths. This keeps the map YAML minimal (just `desc` + `transform`) and makes the vehicle self-contained as a reusable asset:

```yaml
body_mesh: meshes/car_body.emesh
front_wheel_mesh: meshes/car_wheel_front.emesh
rear_wheel_mesh: meshes/car_wheel_rear.emesh
mass: 1200
half_extents: [1.0, 0.5, 2.0]
com_offset: [0.0, -0.3, 0.0]
max_engine_torque: 300
max_steer_angle: 0.5
brake_torque: 1500
handbrake_torque: 3000
front_left:
  position: [-1.0, -0.3, 1.5]
  radius: 0.35
  ...
```

### Map YAML entry
```yaml
- name: player_car
  type: vehicle
  desc: vehicles/car01.vehicle.yaml
  transform: [1, 0, 0, 0, ...]
```

### Scene hierarchy
`GameVehicle` owns its children as `std::unique_ptr<GameMesh>` members and adds them via `GameObject::AddChild()` in `Create()`. `OnAddedToScene()` / `OnRemovedFromScene()` are called manually on children (they are not root objects in GameSystem). Transform propagation from physics and editor edits flows naturally through the existing hierarchy system.

### Wheel mirroring
Right-side wheels share the same `MeshTemplate` as their left-side counterpart but are given a `mirror_x` transform (`scale.x = -1`). In `Update()`, wheel local transforms are computed as `body_world_inv * wheel_world` for left wheels and `body_world_inv * wheel_world * mirror_x` for right wheels, preserving the flip across frames.

### Wheel positions before physics is active
In `Create()`, wheel `SetLocalTransform()` is called using `WheelDesc::position` so the meshes appear at the correct locations in the editor (before `Activate()` runs). After `Activate()`, `Update()` overwrites these with physics results every frame.

### PlayModeManager vehicle wiring
`EnterVisitor` stores the first `GameVehicle*` found in the scene. `Enter()` creates a `PlayerVehicleController`, assigns it to the vehicle, then calls `Activate()`. The vehicle controller pointer is released in `Exit()` before the scene callback fires; the vehicle itself is deactivated later by `OnRemovedFromScene()` during scene teardown.

### Local bbox
`GameObject` requires a local bbox at construction time. Since `GameVehicle` constructs via a static `Create()` factory, the bbox is derived from `VehicleDesc::half_extents` after YAML parsing and passed to the private constructor.

## Output to keep in mind

- Map YAML key for vehicles: `type: vehicle`, `desc: <relative-to-data-root path>`.
- Vehicle desc YAML lives in a `vehicles/` subfolder under the data root by convention.
- Wheel indices in `PhysicsVehicle::GetWheelWorldTransform(i)`: 0=FL, 1=FR, 2=RL, 3=RR (same as `PhysicsSystem::CreateVehicle`).
- `GameVehicle::Update()` calls `controller_->Update(dt)` — the controller is NOT updated by `GameSystem` separately.
- PlayModeManager currently wires the **first** found vehicle. Multiple vehicles would need a different strategy.

## Skills / instructions followed

- `impl-issue` skill
- `src/CLAUDE.md`: one class per `.h`/`.cpp` pair, include root is `src/`, Google C++ style guide, `cppcheck-suppress unusedStructMember` for private data members
- `src/game/CLAUDE.md`: `GameObject` subclasses are scene objects; `OnAddedToScene`/`OnRemovedFromScene` register/unregister renderer resources; no platform or OpenGL headers directly
- `src/physics/CLAUDE.md`: no Jolt types in public headers
- `src/editor/CLAUDE.md`: editor is the leaf of the dependency graph; no editor code included by `game/`
