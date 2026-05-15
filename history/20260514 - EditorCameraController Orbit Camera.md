# EditorCameraController Orbit Camera

**Date:** 2026-05-14
**Issue:** #167
**Branch:** `feat/editor-camera-controller`
**PR:** #185

## What changed

### `src/core/MouseButton.h`

Added `kMiddle` between `kLeft` and `kRight`. The MMB was previously unmapped (fell through to `kRight` silently). This is a non-breaking change since the enum is `uint8_t`-backed and all existing code only checked `kLeft`/`kRight`.

### `src/gldevices/GLDevices.cpp`

Updated `OnMouseButton` to map `GLFW_MOUSE_BUTTON_MIDDLE` → `core::MouseButton::kMiddle`.

### `src/editor/EditorCameraController.h` / `.cpp`

Implements `game::ICameraController` for an orbit-style editor camera.

**Spherical coordinate representation:**
- `focus_` — world-space orbit target (default: origin)
- `azimuth_` — horizontal rotation in radians (default: 0)
- `elevation_` — vertical angle in radians, clamped to `(−π/2+ε, π/2−ε)` (default: 0.3 ≈ 17°)
- `distance_` — orbit radius, clamped to `[0.1, 1000]` (default: 15)

**Controls (OnEvent):**
- Alt+LMB drag: `azimuth += dx * 0.005`, `elevation -= dy * 0.005`
- MMB drag: `focus += right * dx * (distance * 0.001)`, `focus -= kAxisY * dy * (distance * 0.001)`
  - `right = kAxisY × forward` (forward = spherical coords, not from eye)
- Scroll wheel: `distance *= pow(0.9, wheel_delta)`

**Update():**
```
eye = focus + distance * (cos_el*sin_az, sin_el, cos_el*cos_az)
forward = normalize(focus - eye)
right = normalize(kAxisY × forward)
world_matrix = [right | kAxisY | -forward | eye]
```

**Additional API:**
- `SetViewportHovered(bool)` — gates mouse input; EditorViewport calls this before Update()
- `SetFocus / SetDistance / FrameObject(BBox3)` — programmatic camera positioning

### `src/editor/CMakeLists.txt`

Added `EditorCameraController.cpp`.

## Decisions

- **`has_prev_mouse_` reset on drag start**: Prevents a large first-frame delta if the cursor was far from its last tracked position. Cleaner than the `-1.f` sentinel used by FPSCameraController (which could technically yield a wrong delta at x=0).

- **Pan direction uses spherical forward, not Update() forward**: In `OnEvent`, I compute the `right` vector directly from azimuth/elevation for consistency (avoids storing an extra `right_` member). The calculation is cheap.

- **Pan uses world-Y for vertical axis**: Rather than computing the true camera-up vector (`right × forward`), using `kAxisY` for vertical pan simplifies the math. At high elevations the pan direction is slightly inaccurate but acceptable for an initial implementation.

- **Zoom uses `pow(0.9, wheel_delta)`**: Handles fractional wheel deltas (trackpad) smoothly. At elevation near poles, the kAxisY.Cross(forward) would degenerate, but elevation clamping prevents this.

- **`kMiddle` insertion order**: Placed between `kLeft` and `kRight` — the uint8_t value of `kRight` changes from 1 to 2. Any code that stores raw enum values (serialization, etc.) would be affected, but no such code exists yet.

## Output to keep in mind

- `EditorCameraController` is not yet wired to EditorSystem or EditorScene — that happens in issue #170 (default editor scene)
- `SetViewportHovered(bool)` must be called by EditorViewport (issue #169) each frame
- The world matrix convention: col0=right, col1=kAxisY, col2=−forward, col3=eye — matches `FPSCameraController` and `GameCamera::OnWorldTransformUpdated()`
- Next: render-to-texture for the viewport (#168) then EditorViewport (#169)

## Skills used

- `projectSettings:impl-issue`

## CLAUDE.md notes considered

- `src/CLAUDE.md`: one class per `.cpp`/`.h`
- `game/CLAUDE.md`: `ICameraController` — do not consume from EventManager directly, let the system route events
- Root `CLAUDE.md`: `feat/` prefix, conventional commit, cpplint + cppcheck before commit, history file required
