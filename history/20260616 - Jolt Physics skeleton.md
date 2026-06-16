# Jolt Physics External Dependency & Physics Module Skeleton

**Date:** 2026-06-16  
**Issue:** #559  
**Branch:** feat/physics-jolt-skeleton-559

---

## What changed

### `CMakeLists.txt` (root)
Added a `FetchContent_Declare` + `FetchContent_MakeAvailable` block for **Jolt Physics v5.5.0** (`JoltPhysics/JoltPhysics`). All Jolt-internal build targets (samples, performance test, unit tests, docs) are disabled via cache variables to minimize build time.

### `src/CMakeLists.txt`
Added `add_subdirectory(physics)` immediately before `game`, matching the dependency ordering `app → game → physics → core`.

### `src/physics/CMakeLists.txt` *(new)*
Defines the `physics` static library:
- `target_include_directories(physics PUBLIC ...)` — enables `#include "physics/Foo.h"` from consumers.
- `target_include_directories(physics PRIVATE ${JoltPhysics_SOURCE_DIR})` — Jolt headers are **private**, preventing them from leaking into `game/` or `editor/` consumers.
- `target_link_libraries(physics PUBLIC core PRIVATE Jolt)` — `core` is public (consumers get it transitively); `Jolt` is private (its symbols are implementation details).

### `src/physics/Physics.cpp` *(new)*
Minimal placeholder translation unit so CMake's `STATIC` library target has at least one source file. Declares an empty `physics` namespace. Downstream issues replace/augment this.

### `src/physics/CLAUDE.md` *(new)*
Documents the module's role, dependency graph, file table stub, and the key constraint that Jolt headers must never appear in public `physics/*.h` headers.

---

## Decisions and rationale

| Decision | Rationale |
|---|---|
| Jolt v5.5.0 | Latest stable tag as of 2026-06-16 |
| `PRIVATE` Jolt link + include | Prevents Jolt types from leaking into `game/`/`editor/` headers (core constraint from issue) |
| Placeholder `Physics.cpp` | CMake `STATIC` requires ≥1 source; avoids switching to `INTERFACE` which would prevent PRIVATE link deps |
| `core` linked PUBLIC | Consumers of `physics` also need `core` types (vectors, etc.) — avoids duplication |

---

## Things to keep in mind for next features

- The `Jolt` CMake target name is `Jolt` (capital J, no namespace prefix).
- `JoltPhysics_SOURCE_DIR` (from FetchContent) is the include root; Jolt headers are included as `#include <Jolt/Jolt.h>`.
- When adding real source files, add them to the `physics STATIC` target in `src/physics/CMakeLists.txt` and remove the placeholder `Physics.cpp` comment.
- `game/` does not yet link `physics` — that wiring happens in a downstream issue when `PhysicsSystem` provides a real API.
- The Jolt library requires `JPH_USE_SSE4_2` or similar defines on some platforms; check Jolt's own CMake options if physics sim crashes on non-SSE machines.

---

## Skills and CLAUDE.md instructions used

- **Skill:** `impl-issue`
- **CLAUDE.md instructions followed:**
  - Checkout `dev` and pull before anything else (from impl-issue skill instructions)
  - Branch prefix `feat/`
  - Conventional commits
  - PR to `dev` with `Close #559`
  - History markdown in `history/`
  - Google C++ style guide (verified with cpplint)
  - Include root is `src/` (all `#include` paths project-relative)
  - New modules need a `CMakeLists.txt` and entry in `src/CMakeLists.txt`

---

## README.md update proposal

Add Jolt Physics to the "External dependencies" section:

```
| Jolt Physics | v5.5.0 | Rigid-body physics simulation | https://github.com/jrouwe/JoltPhysics |
```

Note: no manual installation required — FetchContent downloads it automatically at CMake configure time.

---

## CLAUDE.md improvement proposals

**`src/physics/CLAUDE.md`** is new and self-documenting.

**Root `CLAUDE.md`:** Consider adding a note that the CMake binary dir is `_build/` and that FetchContent artifacts land in `external/<name>-src/` — several contributors have placed fetched sources in the wrong directory.

**`src/CLAUDE.md`:** Consider adding `physics` to the module list now that it exists.
