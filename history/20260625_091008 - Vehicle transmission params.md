# Vehicle Transmission Parameters — engine_inertia, gear_switch_time, clutch_strength

**Branch:** `feat/vehicle-transmission-params`  
**PR:** #787  
**Issue:** #784

## Changes

### `src/physics/VehicleDesc.h`
Added three new fields to `VehicleDesc` with defaults matching the previously hardcoded values in `PhysicsSystem.cpp`:
```cpp
float engine_inertia   = 0.1f;
float gear_switch_time = 0.1f;
float clutch_strength  = 40.0f;
```
Each has a `// cppcheck-suppress unusedStructMember` annotation following the existing pattern in that struct.

### `src/physics/PhysicsSystem.cpp`
Replaced three hardcoded constants at lines 719–721 with reads from `desc`:
```cpp
ctrl->mEngine.mInertia              = desc.engine_inertia;
ctrl->mTransmission.mSwitchTime     = desc.gear_switch_time;
ctrl->mTransmission.mClutchStrength = desc.clutch_strength;
```
No other changes in `PhysicsSystem.cpp`.

### `src/game/VehicleTemplate.cpp`
Three optional YAML reads added after `handbrake_torque` in the `pd` block, using the same `as<float>(default)` fallback pattern — omitting the keys in YAML silently keeps the struct defaults.

### `src/editor/VehicleEditorWindow.cpp`
- **LoadFromYaml**: three reads inside `if (const YAML::Node ph = root["physics"])` block
- **SaveToYaml**: three keys emitted after `handbrake_torque` inside the `physics:` emitter block
- **DrawPhysicsSection**: new `ImGui::SeparatorText("Transmission")` group at the end of the function with `DragFloat` for each parameter and `SetTooltip` reminders of Jolt defaults

### `data/vehicles/hell.vehicle.yaml`
Added explicit `engine_inertia: 0.1`, `gear_switch_time: 0.1`, `clutch_strength: 40.0` inside the `physics:` block as the reference example. `test_box.vehicle.yaml` deliberately left unmodified to verify optional-field backwards compatibility.

## Decisions

- **Defaults match previous hardcoded values**: zero behaviour change for existing vehicles on the first load, allowing incremental per-vehicle tuning.
- **Fields placed in `VehicleDesc` (not a nested struct)**: consistent with `max_engine_torque`, `brake_torque`, `handbrake_torque` which are all flat in the struct. A `TransmissionDesc` sub-struct was not warranted for three fields.
- **Tooltip on `SeparatorText`**: `IsItemHovered()` after a separator works in ImGui; kept short to avoid clutter. The first tooltip was moved below the first DragFloat (not the separator) so it actually fires on hover.

## Keep in mind

- `test_box.vehicle.yaml` has no transmission keys — good regression test for the optional-field path.
- Jolt's real defaults are `mInertia=0.5`, `mSwitchTime=0.5`, `mClutchStrength=10.0` — the struct defaults intentionally diverge to preserve the tuned game feel.

## Skills and instructions used

- `/impl-issue`
- `src/CLAUDE.md` — Google C++ style, one class per file, include root
- `src/physics/CLAUDE.md` — Jolt-free public headers
- `src/editor/CLAUDE.md` — `cppcheck-suppress unusedStructMember` pattern
- Global `CLAUDE.md` — conventional commits, cpplint before commit, PR to `dev`
