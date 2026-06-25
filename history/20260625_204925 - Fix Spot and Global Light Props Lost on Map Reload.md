# Fix: Spot Light and Global Light Properties Lost on Map Reload

**PR**: #806  
**Issue**: #802  
**Branch**: `fix/spot-global-light-props-lost-on-reload`  
**Date**: 2026-06-25

## Problem

`MapLoader::ParseLight()` only read the six common light fields (`color`, `intensity`, `radius`, `cast_shadow`, `shadow_resolution`, `shadow_bias`) and then immediately constructed the `GameLight`. Type-specific fields written by `MapSerializer` were silently discarded, causing any spot-light or global-light edits made in the editor to be lost on map reload.

Affected types and their missing fields:

| Light type | Written by serializer | Previously loaded |
|---|---|---|
| `GlobalLight` | `direction`, `ambient_color` | ✗ |
| `CircleSpotLight` | `direction`, `inner_angle`, `outer_angle`, `range` | ✗ |
| `RectangleSpotLight` | `direction`, `h_angle`, `v_angle`, `range` | ✗ |
| `OmniLight` | `radius` | ✓ (generic `radius`) |

## Root cause

`ParseLight()` in `src/game/MapLoader.cpp` had no `switch` on `light_type`; the asymmetry was entirely on the load side — `MapSerializer` was correct.

## Fix

Added a `switch (light_type)` block immediately after the `shadow_bias` parse and before the `GameLight` constructor call in `ParseLight()`. Each case reads its type-specific fields using `core::ParseVec3` (already available via `core/YamlSerialiser.h`) and `YAML::Node::as<float>()`.

No new includes required. Only `src/game/MapLoader.cpp` was touched (+31 lines).

## Decisions

- Kept the same guard style as the common block (`if (node["key"])` before reading) so missing keys use the `GameLightDesc` default values, making old map files forward-compatible.
- `kOmni` falls through to `default: break` — it has no extra fields beyond the common `radius`.

## Output / notes for future work

- `MapSerializer` and `MapLoader` are now fully symmetric for all four light types.
- If a new light type is introduced, both `MapSerializer::SerializeVisitor::Visit(GameLight&)` and `ParseLight()` must be updated together.

## Skills / CLAUDE.md instructions applied

- `impl-issue` skill workflow: checkout `dev`, pull, branch from `feat/`/`fix/`, implement, cpplint, commit (conventional commits), PR to `dev` with `Closes #NNN`.
- `src/CLAUDE.md`: Google C++ style guide, include root is `src/`.
- `src/game/CLAUDE.md`: `MapLoader` is a static utility; structs used as data holders may be header-only.
