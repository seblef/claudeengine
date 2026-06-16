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
| *(to be filled in as downstream issues are implemented)* | |

## Guidelines

Follow all rules in `src/CLAUDE.md`. Additionally:

- **Jolt types must not appear in any `physics/*.h` included by `game/` or `editor/`.**  
  Keep all `#include <Jolt/...>` in `physics/*.cpp` and private headers (e.g. `physics/internal/`).
- Public headers expose only engine-native types (e.g. `core::Vec3`, plain POD structs, or forward declarations of opaque handles).
- `physics/` must not include any header from `game/`, `renderer/`, `editor/`, or `gldevices/`.
- One class per `.h` / `.cpp` pair; utility functions go in `*Utils.cpp`.
