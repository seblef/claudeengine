# GameSystem singleton

**Date:** 2026-05-13
**Issue:** #131
**PR:** #142
**Branch:** feat/game-system

## Changes

- `src/game/GameSystem.h` — declares GameSystem (Singleton<GameSystem>); constructor takes `abstract::Devices*`; exposes `Update()`, `AddObject`, `RemoveObject`, `SetCamera`, `SetCameraController`, `IsRunning`, `GetElapsedTime`
- `src/game/GameSystem.cpp` — implements frame sequence: dt computation, event pump, event routing, controller update, renderer update; object and camera management
- `src/game/CMakeLists.txt` — added `GameSystem.cpp`

## Decisions

**dt computed internally:** `Update()` calls `steady_clock::now()` and subtracts `prev_time_`, keeping `main.cpp` free of any timing logic.

**Event routing contract:** `kWindowClose` sets `running_ = false`; all other events go to the controller (if set). This prevents controllers from ever seeing window-close events and ensures only GameSystem decides when to stop.

**SetCamera / SetCameraController cross-binding:** `SetCamera` calls `controller->SetCamera(camera)` when a controller already exists, and `SetCameraController` calls `controller->SetCamera(active_camera_)` when called later — whichever order the caller uses, the binding is always established correctly.

**erase-remove idiom in RemoveObject:** Keeps `objects_` compact without invalidating indices. Objects are stored as raw pointers; GameSystem does not own them.

**`Renderer::SetCamera` in SetCamera:** The renderer needs the camera pointer to fill the scene CB in `Update()`. Pre-registering it via `SetCamera` is consistent with how `main.cpp` uses the renderer directly.

## Output to keep in mind

- `main.cpp` must call `video->BeginFrame()` and `video->ClearRenderTargets()` before `GameSystem::Update()`, or this responsibility needs to move into `Renderer::Update()`. Currently the renderer does not call BeginFrame — see issue #132.
- `GameSystem` does not own any `GameObject*`, `GameCamera*`, or `ICameraController*` — callers manage lifetime.
- `Singleton<GameSystem>` lifecycle: `new GameSystem(devices)` → loop on `IsRunning()` → `GameSystem::Shutdown()`.

## Skills / instructions used

- `src/CLAUDE.md`: one class per file; Google C++ style; include root is `src/`
- `src/game/CLAUDE.md`: GameSystem role and event routing contract
- Root `CLAUDE.md`: git workflow, conventional commits, history file requirement
