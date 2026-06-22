# PlayModeManager — Play-in-Editor Lifecycle Controller

**Issue**: #707 — editor: add PlayModeManager  
**Branch**: `feat/editor-play-mode-manager-707`  
**Milestone**: M1 — Play-in-Editor

---

## Summary

Implemented `PlayModeManager`, which orchestrates the full Enter/Exit lifecycle
for Play-in-Editor mode. It validates the scene, auto-saves, installs an FPS
camera at the GamePlayerStart, registers static physics bodies for all scene
meshes, and restores the editor state on Stop.

---

## Files Created

### `src/editor/PlayModeManager.h`
- Declares `PlayModeManager` with the constructor
  `(EditorScene*, EditorToolbar*, EditorViewport*, abstract::VideoDevice*)`.
- Private nested class `EnterVisitor : public game::GameObjectVisitor` handles
  the single-pass scan for `GamePlayerStart`, `GameTerrain`, and `GameMesh`
  without requiring `GetType()` switches.
- Exposes `Enter()`, `Exit()`, `Tick(float dt)`, `OnEvent(const core::Event&)`,
  `IsPlaying()`, `AreToolsFrozen()`, `ArePanelsHidden()`.
- Two callbacks wired by `EditorWindow`: `SetOnSceneReloaded` and
  `SetOnStatusMessage`.

### `src/editor/PlayModeManager.cpp`
Implements each lifecycle step as described in the issue:

**`Enter()`**:
1. Guard: empty file path → status-bar warning, return.
2. `EnterVisitor` scans `scene_->GetObjects()` to locate `GamePlayerStart`
   and build play-time physics bodies.
3. Guard: no `GamePlayerStart` → status-bar warning, return.
4. `MapSerializer::Save(...)` auto-save; on failure → status-bar error, return.
5. Saves camera state for restoration in `Exit()`.
6. Calls `toolbar_->SetActiveTool(EditorTool::kSelection)` and sets
   `tools_frozen_ = true`, `panels_hidden_ = true`.
7. Creates `FPSCameraController`, binds to viewport's `GameCamera` via
   `GetCamera()`, sets position from player start world transform.
8. Calls `viewport_->SetInPlayMode(true)` to freeze the orbit camera
   controller inside `EditorViewport::Render()`.
9. Stores static physics bodies from the visitor in `play_bodies_`.
10. Terrain body already managed by `GameTerrain::OnAddedToScene()` — no
    duplicate registration.

**`Exit()`**:
1. Destroys all `play_bodies_` via `PhysicsSystem::DestroyBody`.
2. `MapSerializer::Load(...)` to reload scene; fires `on_scene_reloaded_`
   callback on success; logs error on failure (panels still restored).
3. Calls `viewport_->SetInPlayMode(false)` and restores camera state.
4. Resets flags: `panels_hidden_`, `tools_frozen_`, `playing_`.
5. Calls `toolbar_->SetInPlayMode(false)`.

**`Tick(float dt)`**: steps `PhysicsSystem`, then calls
`fps_controller_->Update(dt)` so the camera is current before
`EditorViewport::Render()` draws the frame.

**`OnEvent()`**: forwards to `fps_controller_->OnEvent()` while playing.

**Physics body creation for meshes** (`EnterVisitor::Visit(GameMesh&)`):
- Skips meshes that already have a live physics body (their `GetPhysicsBody()`
  is non-null, meaning `SetPhysicsDesc()` was used in the editor).
- For meshes with CPU geometry: creates a `ConvexHull` body via
  `PhysicsSystem::CreateBodyWithMesh`, reusing the `MeshTemplate*` as the
  shape cache key to deduplicate geometry builds across instances.
- Fallback: box from world bbox half-extents via `PhysicsSystem::CreateBody`.

---

## Files Modified

### `src/editor/EditorViewport.h` / `.cpp`
- Added `GetCamera() const` — returns the viewport's `GameCamera*` so
  `PlayModeManager` can bind the `FPSCameraController` without storing it
  inside the viewport.
- Added `SetInPlayMode(bool)` / `in_play_mode_` member.
- In `Render()`:
  - Skips `camera_ctrl_->SetViewportHovered()` and `camera_ctrl_->Update()`
    when in play mode (prevents the orbit camera from overwriting the FPS
    camera position on the same frame the renderer draws).
  - Skips tool rendering, physics debug draw, gizmos, and the ImGuizmo
    ViewManipulate orbit widget when in play mode.
- In `OnEvent()`: returns immediately when in play mode (events are routed to
  `PlayModeManager::OnEvent()` by `EditorWindow`).

### `src/editor/EditorWindow.h`
- Added `#include "editor/PlayModeManager.h"`.
- Added `std::unique_ptr<PlayModeManager> play_mode_` member (declared after
  other members, before the note about destruction order).
- Added `play_status_msg_` and `play_status_timer_` for transient status-bar
  messages from `PlayModeManager`.

### `src/editor/EditorWindow.cpp`
- **Constructor**: constructs `PlayModeManager` after `viewport_` (dependency
  order), wires `SetOnStatusMessage` and `SetOnSceneReloaded` callbacks.
  - `SetOnSceneReloaded` replaces `scene_`, calls `viewport_->SetScene()`,
    rebuilds `MapPropertiesWindow`, clears command history, re-wires terrain
    panel and environment panel.
  - `SetOnPlay` calls `play_mode_->Enter()` then updates toolbar play mode
    state; `SetOnStop` calls `play_mode_->Exit()`.
- **`OnEvent()`**: routes to `play_mode_->OnEvent()` when playing, else to
  `viewport_->OnEvent()` (same as before).
- **`Render()`**:
  - Introduces local `const float dt = ImGui::GetIO().DeltaTime` used
    throughout.
  - Calls `play_mode_->Tick(dt)` before the viewport panel is rendered (so
    the FPS camera update precedes `Renderer::Update()`).
  - Decrements `play_status_timer_` each frame.
  - `SetPlayEnabled` now also gates on `!play_mode_->IsPlaying()` so Play
    can't be clicked again while already playing.
  - Tool-change dispatch block guarded by `!play_mode_->AreToolsFrozen()`.
  - Scene panel, Properties panel, and Logs panel wrapped in
    `!play_mode_->ArePanelsHidden()`.
  - Status bar shows `play_status_msg_` while `play_status_timer_ > 0`.

### `src/editor/CMakeLists.txt`
- Added `PlayModeManager.cpp` to the editor static library.

---

## Decisions and Rationale

### EditorViewport owns the camera; PlayModeManager drives it via GetCamera()
The spec's "register via `GameSystem::SetCameraController()`" was aspirational
for game-app integration. In the editor, `GameSystem::Update()` is not called;
the orbit camera controller is updated directly by `EditorViewport::Render()`.
Bypassing it via `SetInPlayMode(true)` (which suspends `camera_ctrl_->Update()`)
is the minimal, correct solution that does not mutate `GameSystem` state.

### Terrain physics handled by GameTerrain — no second body in PlayModeManager
`GameTerrain::OnAddedToScene()` already creates a terrain body when
`PhysicsSystem` is instanced. Creating a second body in `Enter()` would double
the terrain collision. The spec's step 9 ("store the handle in `terrain_body_`")
is therefore a no-op — the handle is already inside `GameTerrain`.

### ConvexHull for meshes with CPU geometry, Box fallback for procedural meshes
ConvexHull is cheaper at query time than Exact (triangle mesh) and acceptable
for player collision against environment meshes. The `MeshTemplate*` is passed
as `shape_cache_key` to `CreateBodyWithMesh` so all instances sharing a
template reuse the same Jolt shape.

### HUD skipped — UIRenderer not instanced in the editor app
`ui::UIRenderer` is not created by `editor_app/main.cpp`. Calling
`HUDScreen::Show()` would crash. The `panels_hidden_` flag already provides
a full-viewport game feel; HUD integration is deferred to a follow-up issue.

### Scene reload in Exit() fires the on_scene_reloaded_ callback
The callback design (vs returning a value from Exit()) matches the existing
`EditorScene` callback pattern and allows `EditorWindow` to remain the sole
owner of the `scene_` pointer. Panels are restored even when the reload fails
to avoid a frozen editor state.

---

## Output to Remember for Next Features

- `PlayModeManager` is now fully integrated. The next milestone steps likely
  include: character/player logic, HUD integration (once UIRenderer is wired
  into the editor app), and weapon/pickup systems.
- The `SetInPlayMode(bool)` flag on `EditorViewport` is a clean extension
  point — any future in-editor runtime feature that needs to suppress editor
  overlays can reuse it.
- `PlayModeManager::SetOnSceneReloaded` receives both the new scene and the
  saved camera state; the camera state is currently ignored by the callback
  (it's passed to `viewport_->SetCameraState()` by `Exit()` directly). A
  future refactor could move all restoration into the callback.

---

## Skills Used
- `impl-issue`

## Instructions from CLAUDE.md / Skills Files Used
- One class per `.h` / `.cpp` pair (nested `EnterVisitor` is an exception as
  it is private and defined within `PlayModeManager`).
- `cppcheck-suppress unusedStructMember` added to all private member fields.
- Google C++ style guide followed; `cpplint` passes with no warnings.
- No comments on the "what"; comments only explain non-obvious invariants
  (e.g. why `GameTerrain`'s body is not re-registered).
- Conventional commit message used.
