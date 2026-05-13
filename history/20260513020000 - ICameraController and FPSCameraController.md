# ICameraController and FPSCameraController

**Date:** 2026-05-13
**Issue:** #130
**PR:** #141
**Branch:** feat/fps-camera-controller

## Changes

- `src/game/ICameraController.h` — pure abstract interface: `SetCamera`, `OnEvent`, `Update`; delete copy/move; protected default constructor
- `src/game/FPSCameraController.h` — concrete FPS controller declaration with key flags, yaw/pitch/position state, mouse prev-position tracking
- `src/game/FPSCameraController.cpp` — `OnEvent` handles kKeyDown/kKeyUp/kMouseMoved; `Update` computes look+right vectors, moves position, builds Mat4f from columns, calls `SetWorldTransform`
- `src/game/CMakeLists.txt` — added `FPSCameraController.cpp`

## Decisions

**Column-based Mat4f construction:** `Update()` builds the world transform directly as `Mat4f(right | kAxisY | -look | position)`. This is the exact inverse of what `GameCamera::OnWorldTransformUpdated()` extracts (col3=pos, -col2=forward, col1=up), making the round-trip lossless.

**No-op guard in Update:** `if (!camera_) return` avoids a crash when `Update` is called before `SetCamera`, which can happen during GameSystem initialisation sequencing.

**cppcheck-suppress on static constexpr members:** cppcheck checks headers in isolation and reports `kMoveSpeed`/`kMouseSensitivity` as unused (they're only referenced in the .cpp). Added `// cppcheck-suppress unusedStructMember` following the existing convention across the codebase.

**Protected default constructor on ICameraController:** Prevents direct instantiation while still allowing subclasses to use the default ctor without boilerplate.

## Output to keep in mind

- `GameSystem` (issue #131) must call `controller->OnEvent(e)` for every event it drains from `EventManager`, then call `controller->Update(dt)` once per frame.
- The FPS controller drives `GameCamera` via `SetWorldTransform`, never via the camera's own setters — this is intentional; all spatial updates go through the `GameObject` transform path.
- Future controllers (orbit, follow-target) implement `ICameraController` and plug into `GameSystem` without any other changes.

## Skills / instructions used

- `src/CLAUDE.md`: one class per file; Google C++ style; include root is `src/`
- `src/game/CLAUDE.md`: dependency graph; ICameraController role; event routing convention
- Root `CLAUDE.md`: git workflow, conventional commits, history file requirement
