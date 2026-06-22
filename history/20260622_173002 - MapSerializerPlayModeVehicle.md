# Map Serialisation & PlayMode Vehicle Wiring — Issue #715

**Date:** 2026-06-22  
**Branch:** `feat/map-serialization-play-mode-715`  
**PR:** [#737](https://github.com/seblef/claudeengine/pull/737)  
**Milestone:** M2 — Driveable Car

---

## What was done

### Already present on `dev` (no changes needed)

- `game/MapLoader.cpp` — `ParseVehicle()` already handles `type: vehicle` with `desc`, `name`, and `transform` fields.
- `editor/MapSerializer.cpp` — `SerializeVisitor::Visit(GameVehicle&)` already emits `type: vehicle`, `desc`, and `transform`.

### New changes

**`src/editor/EditorToolbar.h`**
- Added `kCreateVehicle` to the `EditorTool` enum.
- Extended `IsCreationTool()` helper to include `kCreateVehicle`.

**`src/editor/EditorToolbar.cpp`**
- Added a "Create Vehicle" entry (car icon `ICON_FA_CAR`) to the `kCreationTools` array.

**`src/editor/EditorWindow.cpp`**
- Added includes: `game/GameVehicle.h`, `game/VehicleTemplate.h`.
- In the creation-tool activation block, handled `kCreateVehicle`:
  - Opens a synchronous NFD dialog filtered to `*.vehicle.yaml`.
  - On success: resolves the relative path from the data folder, calls `VehicleTemplate::GetOrLoad`, constructs a `GameVehicle`, and creates a `PlacementTool`.
  - On cancel or load failure: reverts toolbar to `kSelection`.

**`src/editor/PlayModeManager.h`**
- Forward-declared `game::ChaseCameraController` (replacing `game::FPSCameraController`).
- Replaced `std::unique_ptr<game::FPSCameraController> fps_controller_` with `std::unique_ptr<game::ChaseCameraController> chase_controller_`.
- Updated class docstring to reflect chase-camera / vehicle-required semantics.

**`src/editor/PlayModeManager.cpp`**
- Replaced `#include "game/FPSCameraController.h"` with `#include "game/ChaseCameraController.h"`.
- **`Enter()`**: Added vehicle-presence validation after player-start check; removed FPS camera creation; activates vehicle, wires `PlayerVehicleController`, creates `ChaseCameraController` with the viewport camera targeting the vehicle.
- **`Exit()`**: Clears `vehicle_->SetVehicleController(nullptr)` before resetting `vehicle_controller_`; calls `chase_controller_.reset()` after viewport restore.
- **`Tick()`**: New ordering:
  1. `vehicle_controller_->Update(dt)` — steer ramp update (before physics)
  2. `PhysicsSystem::Step(dt)` — physics step; `OnBodyTransformUpdated` fires
  3. `vehicle_->Update(dt)` — wheel mesh sync from completed step + feed next step inputs
  4. `chase_controller_->Update(dt)` — camera follows vehicle's new transform
- **`OnEvent()`**: Removed `fps_controller_->OnEvent(event)` (controller no longer exists).

---

## Design decisions

### FPS camera fully removed (not kept as fallback)

The issue makes `GameVehicle` a required precondition for entering play mode. Keeping an FPS fallback would have created dead code and hidden misconfigured scenes. The validation message is shown to the user instead.

### Vehicle::Update() called after physics (not before)

The issue's pseudocode shows `GameVehicle::Update(dt)` called via `GameSystem::Update(dt)`, which runs after the physics step. This ensures wheel mesh transforms reflect the current step's result. The steer-ramp value (from `PlayerVehicleController::Update`) is fed to the physics vehicle a frame later, which is the accepted tradeoff for visual correctness of the wheels.

### `vehicle_->SetVehicleController(nullptr)` in Exit()

The vehicle's non-owning pointer to the controller is cleared before `vehicle_controller_.reset()` to prevent any in-flight physics callback from touching a freed object during the teardown window between Exit() and scene destruction.

### No `GameSystem::SetCameraController()` in the editor

The editor bypasses `GameSystem` (not instanced in the editor app). `ChaseCameraController::Update(dt)` is called directly in `Tick()`, mirroring how `FPSCameraController::Update(dt)` was called before.

---

## Files touched

| File | Change |
|---|---|
| `src/editor/EditorToolbar.h` | Added `kCreateVehicle` + updated `IsCreationTool()` |
| `src/editor/EditorToolbar.cpp` | Added vehicle entry to `kCreationTools` |
| `src/editor/EditorWindow.cpp` | Added vehicle includes + `kCreateVehicle` activation handler |
| `src/editor/PlayModeManager.h` | Replaced `fps_controller_` with `chase_controller_`; updated docs |
| `src/editor/PlayModeManager.cpp` | Updated include; refactored Enter/Exit/Tick/OnEvent |

---

## Skills and instructions used

- `impl-issue` skill
- `src/CLAUDE.md` — one class per `.h`/`.cpp`, include root `src/`
- `src/editor/CLAUDE.md` — dependency graph, value members require full includes, prefer `std::find_if`, GUI vs edition logic separation
- `src/game/CLAUDE.md` — scene objects vs components, `ICameraController` pattern
- `CLAUDE.md` (root) — cpplint, conventional commits, history file requirement

---

## Output to keep in mind

- `MapLoader::ParseVehicle` and `MapSerializer::Visit(GameVehicle&)` use the `transform` matrix format (not `position`/`rotation`), consistent with all other object types.
- Play mode now **requires both** a `GamePlayerStart` and a `GameVehicle` in the scene. Missing either will show a status-bar warning and abort.
- `PlayerVehicleController::Update(dt)` must be called **before** `PhysicsSystem::Step` to update the steer ramp; `GameVehicle::Update(dt)` must be called **after** physics to sync wheel mesh transforms.
- `ChaseCameraController` takes a `const GameObject*` target — passing `vehicle_` (a `GameVehicle*`) is fine by implicit upcast.
