# CLAUDE.md — environment module

## Role

The `environment` module owns all environment-simulation systems: sky, weather,
water, wind, day/night cycle, and foliage. It sits between `game` (consumer)
and `core`/`abstract` (utilities).

## Dependency graph

```
game → environment → core, abstract
renderer → environment (renderer/Renderer.cpp includes environment/SkyRenderer.h)
```

The `environment` module must not depend on `renderer`; the dependency is
one-way (`renderer` calls into `environment`, not the other way around).

## Module structure

| File(s) | Responsibility |
|---|---|
| `EnvironmentDesc` | Header-only data struct; loaded from a map YAML section and passed to subsystem constructors |
| `WorldTime` | Tracks in-game time of day, advances with a configurable time scale, exposes sun/moon directions |
| `SkyInfos` | Header-only constant buffer struct for the sky shader (slot 8) |
| `SkyRenderer` | Singleton GPU renderer: Preetham sky, sun/moon disc, star noise |

## Key invariants

- `EnvironmentDesc` is a plain struct with no methods and no `.cpp`.
- All `#include` paths are project-relative from `src/` (e.g. `#include "environment/WorldTime.h"`).
- Sun/moon direction assumes latitude 45° N; full astronomical accuracy is not required.

## Guidelines

Follow all rules in `src/CLAUDE.md`. Additionally:
- One class per `.h` / `.cpp` pair.
- Structs used purely as data holders (e.g. `EnvironmentDesc`) may be header-only.
- Do not include platform or OpenGL headers; go through `abstract/`.
