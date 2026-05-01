# 2026-05-01 — Initialize game engine

## Summary

Bootstrap of the ClaudeEngine C++ project: CMake build system, loguru integration via FetchContent, `core::Logger` wrapper, and the application entrypoint.

## Changes

| File | Action |
|---|---|
| `CMakeLists.txt` | Created — root CMake project, FetchContent for loguru, `BUILD_PROFILE` variable |
| `src/CMakeLists.txt` | Created — wires `core` and `app` sub-targets |
| `src/core/CMakeLists.txt` | Created — `core` static library, profile-specific logging defines |
| `src/core/Logger.h` | Created — `core::Logger` static Init/Shutdown facade |
| `src/core/Logger.cpp` | Created — loguru init, file sink, shutdown |
| `src/app/CMakeLists.txt` | Created — `claude_engine` executable target |
| `src/app/main.cpp` | Created — process entrypoint, TODO stubs for config and engine |

## Decisions and rationale

### FetchContent over git submodule
FetchContent requires no manual `git submodule update --init` step. `FETCHCONTENT_BASE_DIR` is set to `external/` so loguru sources land in `external/loguru-src/`, respecting the repository convention. `GIT_SHALLOW TRUE` keeps first-configure time low.

### No `LOGURU_IMPLEMENTATION` define
The `LOGURU_IMPLEMENTATION` macro pattern is deprecated in current loguru. The library ships `loguru.cpp` and a proper CMakeLists.txt exposing a `loguru` CMake target. `FetchContent_MakeAvailable` compiles `loguru.cpp` internally; `Logger.cpp` only includes `loguru.hpp`.

### Static `core::Logger` methods
Loguru is inherently process-global (signal handlers, thread-name registry, file sinks). A static-method facade is honest about those semantics and avoids passing a logger reference through every subsystem boundary.

### `BUILD_PROFILE` vs `CMAKE_BUILD_TYPE`
`BUILD_PROFILE` is a domain-level concept controlling both compiler optimisation *and* logging verbosity defines together. `CMAKE_BUILD_TYPE` only handles the former. This avoids surprising behaviour when a developer sets `CMAKE_BUILD_TYPE=RelWithDebInfo` for profiling but still wants verbose logging.

### Include root at `src/`
`target_include_directories(core PUBLIC ${CMAKE_SOURCE_DIR}/src)` establishes the convention that all headers are included as `#include "core/Logger.h"`, `#include "abstract/IDevice.h"`, etc. — project-relative, no `../` traversal, consistent with Google Style.

### `core` links loguru `PUBLIC`
Any future module (abstract, renderers, etc.) that links `core` gets loguru transitively. No repeated linkage declarations needed.

## Outputs to keep in mind

- `CMAKE_RUNTIME_OUTPUT_DIRECTORY` is set to `${CMAKE_SOURCE_DIR}/build`; the binary is always `build/claude_engine`.
- `claude_engine.log` is written to the process working directory (wherever the binary is invoked from). A future config-loading step should redirect it to a user-data path.
- The two TODO stubs in `main.cpp` mark where config loading (#1) and engine instantiation (#2) will be wired.
- `src/abstract/` has no CMakeLists.txt yet. Add `add_subdirectory(abstract)` in `src/CMakeLists.txt` when the first abstract class is created.
- Build directory convention: use `_build/` (gitignored) as the CMake binary dir, keeping `build/` clean for just the final binary.

## Build commands

```bash
# Configure
cmake -S . -B _build -DBUILD_PROFILE=dev    # or stable

# Build
cmake --build _build

# Run
./build/claude_engine
```

## Skills and CLAUDE.md instructions used

- Root `CLAUDE.md`: single executable in `build/`, dev/stable profiles, loguru for logging, modern C++, history file requirement, CLAUDE.md update proposals.
- `src/CLAUDE.md`: one class per file, Google C++ style, meaningful documentation on exposed interfaces.

## Proposed CLAUDE.md improvements

### Root CLAUDE.md
- Add a note that the CMake binary dir is `_build/` (not `build/`) — `build/` holds only the final executable.
- Document the `BUILD_PROFILE=dev|stable` CMake variable.

### src/CLAUDE.md
- Add a note that the include root is `src/`, so all `#include` paths are project-relative (e.g. `#include "core/Logger.h"`).
- Mention that new modules need a `CMakeLists.txt` and an entry in `src/CMakeLists.txt`.
