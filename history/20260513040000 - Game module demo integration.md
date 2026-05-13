# Game module demo integration

**Date:** 2026-05-13
**Issue:** #132
**PR:** #143
**Branch:** feat/game-module-demo

## Changes

- `src/app/main.cpp` — full rewrite using game module; ~230 → ~145 lines
- `src/game/GameSystem.h/.cpp` — added `SetEventCallback(std::function<void(const core::Event&)>)`
- `src/game/FPSCameraController.h/.cpp` — added `SetPosition(core::Vec3f)`

## Decisions

**`SetEventCallback` on GameSystem:** Debug key handling (G-buffer channel selection) needs to react to events but isn't a camera controller. Rather than forcing it into a controller or re-exposing the EventManager queue, a callback registered on GameSystem receives every event before controller routing. This keeps `main.cpp` event code to ~5 lines without coupling GameSystem to renderer debug mode.

**`SetPosition` on FPSCameraController:** The controller initialises its `position_` to zero. Setting `{0, 5, 30}` before the first `Update()` is the canonical way to place the camera; the first frame's `Update(dt)` builds the correct transform from that position.

**`BeginFrame`/`ClearRenderTargets` remain in the main loop:** GameSystem owns timing and event routing, but video-device frame management is deliberately left to the caller (or to a future Engine class) to keep GameSystem free of VideoDevice coupling that wasn't in its spec.

**`MeshTemplate` refs released after GameMesh creation:** `GetOrLoad` returns an AddRef'd pointer. Each `GameMesh` constructor calls `AddRef()` again, so calling `Release()` on the template pointer after construction leaves the ref-count at 1 (owned by GameMesh). If mesh loading fails and no GameMesh is created, the template is still released cleanly.

## Output to keep in mind

- The `game::GameSystem::SetEventCallback` hook is the canonical way to handle non-camera, non-lifecycle events in main. Document it in game/CLAUDE.md.
- `FPSCameraController::SetPosition` should be called before `GameSystem::SetCameraController` so the first `Update()` uses the correct position (though calling it after is also fine since the first frame dt is tiny).
- `GameSystem` does not call `BeginFrame`/`ClearRenderTargets` — the caller must do so before `Update()`.
- The scale applied to the OBJ mesh (3×) is now symmetrical with the FBX mesh.

## Skills / instructions used

- `src/CLAUDE.md`: one class per file; Google C++ style; include root is `src/`
- `src/game/CLAUDE.md`: dependency graph; event routing contract
- Root `CLAUDE.md`: git workflow, conventional commits, history file requirement
