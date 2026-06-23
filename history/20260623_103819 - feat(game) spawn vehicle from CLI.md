# feat(game): spawn player vehicle from --vehicle CLI argument

**PR**: #768  
**Issue**: #766  
**Branch**: `feat/spawn-vehicle-from-cli`  
**Date**: 2026-06-23

## Changes

### `src/app/main.cpp`

- Added includes: `ChaseCameraController.h`, `GameVehicle.h`, `PlayerVehicleController.h`, `VehicleTemplate.h`
- Extended CLI arg parsing to recognise `--vehicle <desc_path>` alongside `--map`
- Added a **vehicle spawn section** between map load and camera setup:
  - `VehicleTemplate::GetOrLoad(vehicle_path, video)` — falls back to FPS on failure
  - `GameVehicle` pushed into `map_objects` (ownership) and registered with `game.AddObject`
  - World transform set to `map_player_start->GetWorldTransform()`
  - `PlayerVehicleController` created; assigned via `SetVehicleController`; `Activate()` called
  - `ChaseCameraController` created; `SetCamera(&camera)` + `SetTarget(vehicle_ptr)`
- Updated **camera setup**: added `else if (vehicle_ptr)` branch that uses `game.SetCameraController(chase_controller.get())`; FPS path unchanged
- Updated **event callback**: lambda now captures `player_vehicle_ctrl` and forwards `OnEvent(e)` to it before existing debug-key handling
- Updated **main loop**: `vehicle_ptr->Update(frame_dt)` called inside the existing `map_world_time || map_wind_system || vehicle_ptr` gate
- Added `if (vehicle_tmpl) vehicle_tmpl->Release()` before `PhysicsSystem::Shutdown()` to pair with `GetOrLoad`

### `data/maps/demo.map.yaml`

- Removed `hell_001` vehicle object entry
- Changed `editor.selected_object` from `hell_001` to `Object` (first player start)

## Decisions

**Why ownership in `map_objects`?**  
The issue explicitly asked for this: the vehicle is torn down with the rest of the scene without any special-case cleanup in `main`. `GameVehicle::OnRemovedFromScene()` calls `Deactivate()` automatically.

**Why `VehicleTemplate::Release()` before `PhysicsSystem::Shutdown()`?**  
The explicit `Release()` pairs with the explicit `GetOrLoad()` call in `main`. `GameVehicle`'s own destructor will issue a second `Release()` (its constructor `AddRef`'d), which brings the ref count to 0 and frees the resource. Placing `Release()` before physics shutdown ensures no resource access occurs after subsystem teardown.

**Why not call `chase_controller->Update(frame_dt)` explicitly?**  
`GameSystem::SetCameraController` registers the controller for per-frame `Update()` calls inside `GameSystem::Update()`. Explicit calls would double-tick it.

**Why forward vehicle controller events via `SetEventCallback` rather than `SetCameraController`?**  
`ChaseCameraController::OnEvent` is a no-op by design. Vehicle input goes through `PlayerVehicleController::OnEvent`. The event callback is the right place to multiplex input since the chase controller occupies the `SetCameraController` slot.

## Reference files used

- `src/editor/PlayModeManager.cpp` — exact vehicle activation + chase camera wiring pattern
- `src/game/ChaseCameraController.h` — `SetCamera`, `SetTarget`, `SetArmLength`, `SetArmHeight`
- `src/game/PlayerVehicleController.h` — `OnEvent`, `Update`
- `src/game/VehicleTemplate.h` — `GetOrLoad`, `Release`
- `src/game/GameVehicle.h` — `SetVehicleController`, `Activate`, `Deactivate`

## Skills / CLAUDE.md rules applied

- Checked out `dev` and pulled before branching (`feat/` prefix)
- Ran `cpplint` — no errors
- Conventional commit message
- PR targets `dev` with `Close #766`
- History file written (this file)

## Keep in mind

- `PlayerVehicleController::OnEvent` receives **all** events (not just `kKeyDown`). The controller itself filters; no guard needed in the callback.
- The `--vehicle` flag silently no-ops when the map has no player start — no error is printed for missing player start in vehicle mode (the existing warning "Map has no player start" covers it).
- If a `map_camera` is present in the loaded map, it takes priority over the chase camera. This edge case is unlikely in practice (demo map has no camera object).
