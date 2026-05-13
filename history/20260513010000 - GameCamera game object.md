# GameCamera game object

**Date:** 2026-05-13
**Issue:** #129
**PR:** #140
**Branch:** feat/game-camera

## Changes

- `src/game/GameCamera.h` — declares `GameCamera`, a `GameObject` subclass owning a `core::Camera`
- `src/game/GameCamera.cpp` — implements constructor with defaults, `OnWorldTransformUpdated` (extracts position/forward/up from transform columns), convenience setters
- `src/game/CMakeLists.txt` — added `GameCamera.cpp` to the `game` STATIC library

## Decisions

**Column extraction convention:** The world transform is row-major with `m(row, col)`. For a standard TRS matrix:
- Column 3 → position `{m(0,3), m(1,3), m(2,3)}`
- Column 2 negated → forward `{-m(0,2), -m(1,2), -m(2,2)}` (right-handed −Z convention)
- Column 1 → up `{m(0,1), m(1,1), m(2,1)}`

**No-op OnAddedToScene/OnRemovedFromScene:** The camera is not a `Renderable` and does not participate in the visibility system. `GameSystem` will pass `GetCamera()` directly to `Renderer::Update()`.

**Constructor initialises sensible defaults:** FOV 60°, min depth 0.1, max depth 1000. Screen centre is left at `{0,0}` — the caller must set it via `SetScreenCenter()` once the viewport dimensions are known.

**Each convenience setter calls UpdateMatrices():** Keeps the camera matrices consistent after any single property change, at the cost of recomputing matrices more than strictly necessary when multiple setters are called. Acceptable since this is an infrequent setup-time operation.

## Output to keep in mind

- `GameSystem` (issue #131) must call `camera->GetCamera()` and pass the result to `Renderer::Update()`.
- The FPSCameraController (issue #130) drives the camera via `SetWorldTransform()` — it only needs a `Mat4f` from yaw/pitch and never touches the camera API directly.
- Screen centre must be set to `{width * 0.5f, height * 0.5f}` after the window is created.

## Skills / instructions used

- `src/CLAUDE.md`: include root is `src/`; one class per file; Google C++ style
- `src/game/CLAUDE.md`: dependency graph; key invariants
- Root `CLAUDE.md`: git workflow, conventional commits, history file requirement
