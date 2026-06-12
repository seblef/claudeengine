# fix(editor): keyboard shortcut conflicts (#497)

## Problem

Two categories of keyboard input conflicts existed in the editor:

1. **Textbox typing fired editor shortcuts** — pressing Delete in a rename
   input or any `InputText` widget deleted the selected scene object.
   Ctrl+S / Ctrl+N could save or create a new map while the user was just
   editing a text field.

2. **Camera movement keys responded to Ctrl+key combos** — holding Ctrl and
   pressing a movement key (e.g. Ctrl+W, Ctrl+Z in fly mode) still moved the
   camera, making it hard to use Ctrl-based shortcuts while the viewport was
   focused.

## Root cause analysis

| Location | Issue |
|---|---|
| `EditorViewport::OnEvent()` | Delete handling had no `WantCaptureKeyboard` guard — fired even when a text widget had focus |
| `EditorViewport::OnEvent()` | All key-down events forwarded to `EditorCameraController` unconditionally, including while typing in panels |
| `EditorWindow::RenderMenuBar()` | Ctrl+S / Ctrl+N used `IsKeyChordPressed` without a `WantCaptureKeyboard` guard; ImGui's global routing fires these even when a text widget owns the keyboard |
| `EditorCameraController::Update()` | Movement keys (WASD / QE) applied even when Ctrl was held, conflicting with Ctrl+Z (undo), Ctrl+S (save), etc. |

`EditorToolbar::Render()` was already fully guarded (`!io.WantCaptureKeyboard` wrapping all shortcuts + `!io.KeyCtrl && !io.KeyAlt` on single-key tool shortcuts).

## Changes

### `EditorViewport.cpp`
- Delete-object logic now checks `!ImGui::GetIO().WantCaptureKeyboard`.
- `kKeyDown` events are no longer forwarded to the camera controller when
  `WantCaptureKeyboard` is true. `kKeyUp` events are always forwarded so
  that held-movement flags (`w_down_`, `q_down_`, …) are properly cleared
  and the camera never gets "stuck" moving when the user starts typing.

### `EditorWindow.cpp`
- Ctrl+S and Ctrl+N shortcuts wrapped in `!io.WantCaptureKeyboard` guard.

### `EditorCameraController.h` / `.cpp`
- Added `ctrl_down_` member (mirrors existing `alt_down_` / `shift_down_`).
- Ctrl key state tracked in `OnEvent()` for both left and right Ctrl.
- `ctrl_down_` reset in `kWindowLostFocus` handler alongside the other modifiers.
- Both fly-mode (WASDQE) and orbit-mode (W/S focus translation) movement is
  suppressed while `ctrl_down_` is true, so Ctrl+shortcut combos no longer
  accidentally move the camera.

## Decisions

- `kKeyUp` events are always forwarded to the camera even when
  `WantCaptureKeyboard` is true. This is intentional: it prevents the
  scenario where the user was holding a movement key, then a popup appeared
  (capturing the keyboard), causing the corresponding `kKeyUp` to be
  swallowed and the camera to keep drifting.

- Ctrl suppression covers both fly mode and orbit mode W/S focus
  movement for consistency. Alt is not used as a guard for movement keys
  (Alt+LMB is orbit-drag; suppressing movement on Alt would break
  Alt-orbiting while W/S is held).

## Skills and files used

- `src/CLAUDE.md`, `src/editor/CLAUDE.md`
- `EditorViewport.cpp/.h`, `EditorWindow.cpp`, `EditorCameraController.cpp/.h`
- `EditorToolbar.cpp` (read for reference — already correct)
