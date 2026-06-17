# PlayerConfig — Configurable FPS Movement Constants

**Branch**: `feat/fps-camera-character-controller-568`  
**Date**: 2026-06-17

---

## Summary

The seven movement/physics tuning constants in `FPSCameraController` were
hard-coded as `static constexpr` values. They are now runtime-configurable
via a new `player:` section in `data/config.yaml`. All fields have defaults
matching the prior hard-coded values, so omitting the section is safe.

---

## Changes

### `src/core/PlayerConfig.h` (new)

`PlayerConfig` class following the same pattern as `ShadowConfig` /
`PostProcessConfig`. Private float fields with defaults, public getters,
and a `Parse(const YAML::Node&)` method for the `player:` YAML subtree.

Fields:
| YAML key | Getter | Default | Unit |
|---|---|---|---|
| `capsule_radius` | `GetCapsuleRadius()` | 0.8 | m |
| `capsule_half_height` | `GetCapsuleHalfHeight()` | 1.8 | m |
| `eye_offset` | `GetEyeOffset()` | 0.3 | m |
| `move_speed` | `GetMoveSpeed()` | 10.0 | m/s |
| `mouse_sensitivity` | `GetMouseSensitivity()` | 0.002 | rad/px |
| `jump_speed` | `GetJumpSpeed()` | 7.0 | m/s |
| `gravity` | `GetGravity()` | 9.81 | m/s² |

### `src/core/PlayerConfig.cpp` (new)

`Parse()` implementation: reads each key with `if (node["key"])` guard
(missing keys keep their defaults). Logs all values at `INFO` level on load.

### `src/core/AppConfig.h/.cpp`

- Added `#include "core/PlayerConfig.h"`.
- Added `static PlayerConfig player_` member and `GetPlayer()` getter.
- `Init()` now also parses `root["player"]` if present.

### `src/core/CMakeLists.txt`

Added `PlayerConfig.cpp` to the `core` static library target.

### `src/game/FPSCameraController.h`

- Removed the 7 `static constexpr float k*` constants.
- Added 7 `float` private members (`capsule_radius_`, `capsule_half_height_`,
  `eye_offset_`, `move_speed_`, `mouse_sensitivity_`, `jump_speed_`, `gravity_`),
  each with a `cppcheck-suppress unusedStructMember` annotation.

### `src/game/FPSCameraController.cpp`

- Added `#include "core/AppConfig.h"` and `#include "core/PlayerConfig.h"`.
- Constructor reads `AppConfig::GetPlayer()` and copies all 7 values into
  the corresponding instance members.
- All usages of the former `k*` constants updated to use instance members.

### `data/config.yaml`

Added `player:` section after `shadows:` with all seven keys and their values.

---

## Decisions

| Decision | Rationale |
|---|---|
| `PlayerConfig` in `core/`, not `game/` | `AppConfig` aggregates all configs in `core/`; `game/` cannot be included by `core/`. Putting player config in `core/` keeps the same `AppConfig::GetXxx()` pattern as every other section. |
| Instance members, not constexpr | Values come from YAML at runtime, so `constexpr` is not applicable. Instance members also allow per-instance overrides in tests if needed. |
| Init in constructor | `AppConfig::Init()` is called at app startup before any `FPSCameraController` is constructed, so the config is guaranteed to be populated. |
| All keys optional in YAML | Any missing key keeps its in-code default. This prevents startup crashes when older `config.yaml` files don't have the `player:` section. |

---

## Skills and CLAUDE.md instructions used

- `src/CLAUDE.md`: one class per `.h/.cpp`; Google C++ style; include root is `src/`.
- `src/game/CLAUDE.md`: `game → core` dependency; no platform headers.
