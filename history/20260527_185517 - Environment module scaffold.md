# Environment module scaffold — `EnvironmentDesc` + `WorldTime`

## Summary

Adds a new `src/environment/` static library as the home for all environment
simulation systems. This first scaffold introduces two foundational classes:

- **`EnvironmentDesc`** — header-only data struct holding all environment
  parameters read from a map's YAML section (sky/water/cloud/wind/tree toggles,
  water level, time scale, cloud density, wind strength/direction).
- **`WorldTime`** — tracks in-game time of day (0–86400 s) with a configurable
  time scale multiplier; exposes sun/moon directions and daytime predicate.

## Decisions and rationale

### Sun direction model

The issue specified: *"azimuth from time-of-day and fixed elevation arc
(max elevation at noon ≈ 60°, latitude 45° N assumption). Full astronomical
accuracy is not required."*

Implementation:
- Solar azimuth sweeps a full 2π over 24 h, starting east at 06:00.
- Elevation is a sine arc peaking at noon; `sin(π * (h-6)/12) * 45°` yields
  ≈ 45° max elevation (equinox at lat 45° N) and goes negative from 18:00–06:00.
- The coordinate system is: +X east, +Y up, +Z south — consistent with the
  engine's existing right-handed world space.

### Moon direction

Approximated as the antipodal sun vector with a small fixed rotation
(`kMoonTiltRad = 0.1 rad`) around the X axis. This prevents the moon
from sitting exactly opposite the sun in the sky without adding an
unneeded full orbital model.

### Module placement in dependency graph

`environment` depends on `core` and `abstract` (as specified in the issue).
`game` links `environment`; the chain is:

```
game → environment → core, abstract
```

This keeps the module reachable from the game loop without polluting lower
layers.

## Files changed

| File | Change |
|---|---|
| `src/environment/EnvironmentDesc.h` | New — header-only struct |
| `src/environment/WorldTime.h` | New — class declaration |
| `src/environment/WorldTime.cpp` | New — class implementation |
| `src/environment/CMakeLists.txt` | New — static library definition |
| `src/environment/CLAUDE.md` | New — module documentation |
| `src/CMakeLists.txt` | Add `add_subdirectory(environment)` |
| `src/game/CMakeLists.txt` | Link `environment` into `game` |

## Output to keep in mind for next features

- Subsequent issues (sky, water, wind, foliage) should add their `.cpp`
  files to `src/environment/CMakeLists.txt` and consume `EnvironmentDesc`
  fields directly.
- `WorldTime::GetSunDirection()` returns a unit vector in world space
  (+Y up); sky shaders can use it directly for atmospheric scattering.
- `EnvironmentDesc` has no YAML parsing yet — that will be wired up when
  `MapLoader` is extended to read an `environment:` YAML key.

## Skills and instructions used

- `impl-issue` skill
- `src/CLAUDE.md`: project-relative includes, one class per file, Google C++
  style, cpplint before commit, conventional commit messages, history file
  required.
- `src/game/CLAUDE.md`: dependency graph direction, header-only data structs,
  no platform headers.
