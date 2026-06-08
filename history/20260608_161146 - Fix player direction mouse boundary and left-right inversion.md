# Fix: Player direction mouse boundary and left/right key inversion (#404)

## Changes

### `src/abstract/Devices.h`
Added `SetCursorCapture(bool)` virtual method with a default no-op body. This keeps the abstract interface platform-independent while allowing platform drivers to lock/release the OS cursor.

### `src/gldevices/GLDevices.h` / `GLDevices.cpp`
Implemented `SetCursorCapture` via `glfwSetInputMode(window_, GLFW_CURSOR, ...)`:
- `true` → `GLFW_CURSOR_DISABLED`: hides the cursor and delivers virtual/unbounded mouse deltas even at window/screen edges.
- `false` → `GLFW_CURSOR_NORMAL`: restores the visible OS cursor.

### `src/app/main.cpp`
Added `devices.SetCursorCapture(true)` just before the main game loop. Only the game binary locks the cursor; the editor binary does not, leaving ImGui interactions unaffected.

### `src/game/FPSCameraController.cpp`

#### Bug 1 — Inverted left/right strafe
The `right` vector was computed as `Y.Cross(look)`. For a right-handed system with `look = (0,0,-1)` this yields `(-1,0,0)` — pointing **left**, inverting both the strafe movement and column 0 of the camera world transform. Changed to `look.Cross(Y)` which gives `(+1,0,0)`, the correct rightward direction. Movement flags `k_right_`/`k_left_` and their signs are unchanged; only the direction of `right` itself is corrected.

#### Bug 2 — Direction freezing at window/screen boundary
Added handling of `kWindowLostFocus` to reset `prev_mouse_x_/y_` to `-1.f`. This sentinel causes the controller to skip one delta on the first `kMouseMoved` after focus is restored, preventing a large look-direction jump. The actual boundary fix is GLFW cursor capture (above); the reset is a defensive guard for alt-tab scenarios.

## Decisions & rationale

- **`SetCursorCapture` on `abstract::Devices` (not per-controller)**: the FPS controller lives in `game`, which cannot import `gldevices`. Placing the API on `abstract::Devices` respects the dependency graph while keeping it available to any callee that holds a `Devices` reference.
- **Capture enabled in `app/main.cpp`, not in `GLDevices` constructor**: the same `GLDevices` is used by the editor binary, which needs a free cursor for ImGui. Opting into capture at the application level keeps each entry point in control.
- **Cross-product fix instead of swapping key flag signs**: fixing the vector to be semantically correct (`right` = rightward) is cleaner and makes the transform matrix column 0 meaningful for future readers.

## Skills / CLAUDE.md references used
- `src/CLAUDE.md`: Google C++ style, include paths, module layout.
- `src/game/CLAUDE.md`: dependency graph, one class per file, no platform headers in `game`.
- `src/gldevices/CLAUDE.md`: GLFW callback patterns.

## Output to keep in mind
- `abstract::Devices::SetCursorCapture` is now available for any future feature that needs cursor toggling (e.g., opening an in-game menu).
- The editor binary (`editor_app/main.cpp`) intentionally does **not** call `SetCursorCapture`; if a gameplay preview mode is added to the editor, it should call it then release on exit.
