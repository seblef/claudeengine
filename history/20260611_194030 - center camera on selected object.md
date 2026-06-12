# feat(editor): Center Camera on Selected Object

**Issue**: #494  
**PR**: #496  
**Branch**: `feat/camera-center-on-selected-494`

## Changes

### `src/editor/EditorViewport.h` / `.cpp`
Added `FrameObject(const core::BBox3& bbox)` public method — a thin delegator to
`camera_ctrl_->FrameObject(bbox)`. This keeps the camera controller private while
exposing a clean framing API to `EditorWindow`.

### `src/editor/EditorToolbar.h` / `.cpp`
- New `SetOnCenterOnObject(std::function<void()>)` and `SetCanCenterOnObject(bool)` API
  (matching the existing `SetOnFallToTerrain` / `SetCanFallToTerrain` pattern).
- New `on_center_on_object_` / `can_center_on_object_` private members
  (with `cppcheck-suppress unusedStructMember`).
- Button using `ICON_FA_CROSSHAIRS` appended after the "Fall to Terrain" button in the
  action-tools section; grayed out when `can_center_on_object_` is false, with tooltip
  "Select a non-terrain object first".

### `src/editor/EditorWindow.h` / `.cpp`
- New `CenterCameraOnObject()` private method: guards against null selection and
  terrain type, then calls `viewport_->FrameObject(obj->GetWorldBBox())`.
- Wired as `toolbar_->SetOnCenterOnObject([this]{ CenterCameraOnObject(); })` in the
  constructor (alongside the other toolbar callbacks).
- `can_center_on_object_` computed in the toolbar state block of `Render()`:
  `sel && sel->GetType() != game::GameObjectType::kTerrain`.

## Decisions

- Reused the existing `EditorCameraController::FrameObject(bbox)` — no new camera logic
  needed; it already computes focus and distance from the bbox.
- Used `GetWorldBBox()` (not `GetLocalBBox()`) because the camera operates in world space.
- No keyboard shortcut added — the issue only asked for a toolbar button.
- Terrain exclusion mirrors the "Fall to Terrain" button convention; terrains have no
  meaningful camera framing target (they span the entire scene).

## Notes for future contributions

- `EditorViewport::FrameObject` can be reused if a double-click-to-frame shortcut
  is ever added to the viewport or the Objects panel.
- If a keyboard shortcut (e.g. F key) is desired later, add it to `EditorToolbar::Render()`
  in the keyboard-shortcut block alongside the other tool shortcuts.

## Skills and CLAUDE.md rules applied

- Followed the `SetOnXxx` / `SetCanXxx` toolbar callback pattern from `EditorToolbar.h`.
- One class per `.h`/`.cpp` pair; no new files needed.
- `cppcheck-suppress unusedStructMember` added to all new private non-trivial members.
- Ran `cpplint` before committing; no warnings.
- Conventional commit message: `feat(editor): add toolbar button to center camera on selected object`.
- PR targets `dev`, closes #494.
