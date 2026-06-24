# Vehicle Selection Modal for Play Mode

**Issue**: #767  
**Branch**: feat/editor-vehicle-selection-modal  
**Date**: 2026-06-24

## Changes

### Problem

`PlayModeManager::Enter()` required a `GameVehicle` to be pre-placed in the scene. If none was found by the visitor, it hard-stopped with "Place a Vehicle in the scene before entering Play mode". This coupled play-mode UX to map authoring.

### Solution

Route the Play button through `VehicleSelectionModal` so the user picks a vehicle at play time. The vehicle is spawned at the player start position and activated with `PlayerVehicleController` + `ChaseCameraController`.

## Files Changed

| File | Change |
|------|--------|
| `src/editor/PlayModeManager.h` | `Enter()` → `Enter(const std::string& vehicle_name)`; add `owned_vehicle_`; remove `vehicle` from `EnterVisitor` |
| `src/editor/PlayModeManager.cpp` | Spawn vehicle from name; remove it in `Exit()` via `GameSystem::RemoveObject` |
| `src/editor/EditorWindow.h` | Add `play_vehicle_modal_` member |
| `src/editor/EditorWindow.cpp` | Open modal on Play click; call `Enter(name)` on confirmation |

## Decisions

### Ownership model: GameSystem::AddObject vs EditorScene::AddDynamicObject

The spawned vehicle is registered via `game::GameSystem::Instance().AddObject()` directly instead of `scene_->AddDynamicObject()`. This means:
- The vehicle is rendered and physics-active during play mode ✓
- It does **not** appear in the outliner (EditorScene's dynamic pool) ✓
- `PlayModeManager` retains ownership via `owned_vehicle_` ✓

In `Exit()`, `GameSystem::RemoveObject()` calls `OnRemovedFromScene()` → `Deactivate()` (destroys physics vehicle), then `owned_vehicle_.reset()` destroys the `GameVehicle` and releases the `VehicleTemplate` reference via `~GameVehicle()`.

### Separate modal instance for play mode

`EditorWindow` uses `vehicle_modal_` for kCreateVehicle scene placement, and the new `play_vehicle_modal_` for play mode selection. They are separate instances to avoid ambiguity about which use case triggered the modal result.

### VehicleTemplate lifecycle

After `GetOrLoad()` (ref=1) and `make_unique<GameVehicle>(tmpl)` (GameVehicle AddRef's → ref=2), we immediately call `tmpl->Release()` (ref=1). The vehicle owns its ref. When `owned_vehicle_.reset()` destroys the vehicle, `~GameVehicle()` releases the final ref.

## Key Invariants to Keep in Mind

- `Enter(vehicle_name)` must be called with a valid vehicle basename (e.g. `"hell"`) returned by `VehicleSelectionModal::Render()`.
- `Activate()` must only be called while the vehicle is registered with `GameSystem` (i.e. after `AddObject()`).
- The `EnterVisitor` no longer visits `GameVehicle` objects; pre-placed vehicles in the scene are simply ignored.

## Skills Used

- `impl-issue`

## Instructions Followed

- One class per `.h`/`.cpp` pair (no new files needed)
- Google C++ style guide
- `cppcheck-suppress unusedStructMember` for new member
- Conventional commits for PR
