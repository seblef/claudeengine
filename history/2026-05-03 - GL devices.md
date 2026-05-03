# OpenGL window with GLFW (Issue #16)

## Changes

### `CMakeLists.txt` (modified)

Added GLFW 3.4 via FetchContent, placed before `add_subdirectory(src)` so the `glfw` CMake target is available when `gldevices` is configured. Wayland is disabled (`GLFW_BUILD_WAYLAND OFF`) and X11 is used explicitly — Wayland scanner was not available on the build machine.

### `src/gldevices/CMakeLists.txt` (new)

STATIC library. Links `abstract` as PUBLIC (since `GLDevices.h` inherits from `abstract::Devices`) and `glfw` as PRIVATE (GLFW headers are not exposed to callers).

### `src/gldevices/GLDevices.h` (new)

Concrete `Devices` subclass. Forward-declares `struct GLFWwindow` to keep GLFW headers out of including TUs. Declares seven private static GLFW callbacks and the `GlfwKeyToKey` key-mapping helper.

### `src/gldevices/GLDevices.cpp` (new)

Constructor: calls `glfwInit()`, hints OpenGL 4.6 core profile, creates the window (fullscreen if requested), makes the context current, and registers all seven callbacks. Destructor: destroys the window and calls `glfwTerminate()`.

| Method | Implementation |
|--------|---------------|
| `Update()` | `glfwPollEvents()` |
| `GetWindow()` | returns `window_` as `void*` |
| `GlfwKeyToKey()` | switch on `GLFW_KEY_*` → `core::Key` for all 60 mapped keys; default → `kUnknown` |

Callback mapping:

| GLFW callback | `core::EventType` |
|---------------|-------------------|
| `glfwSetWindowCloseCallback` | `kWindowClose` |
| `glfwSetWindowFocusCallback` | `kWindowGetFocus` / `kWindowLostFocus` |
| `glfwSetWindowIconifyCallback` | `kWindowMinimized` / `kWindowRestored` |
| `glfwSetCursorPosCallback` | `kMouseMoved` (x, y cast to float) |
| `glfwSetScrollCallback` | `kMouseWheelChanged` (y-axis only) |
| `glfwSetMouseButtonCallback` | `kMouseButtonDown` / `kMouseButtonUp` (left/right only) |
| `glfwSetKeyCallback` | `kKeyDown` (press + repeat) / `kKeyUp` |

### `src/CMakeLists.txt` (modified)

Added `add_subdirectory(gldevices)` between `abstract` and `app` so `gldevices` is compiled before the app executable that links it.

### `src/app/main.cpp` (modified)

Replaces the stub with a full event loop: creates a `GLDevices(640, 480, false)` window, calls `Update()` each frame, drains the `EventManager` queue with a `switch` over all `EventType` values, logs each event with its payload, and exits on `kWindowClose`.

### `src/app/CMakeLists.txt` (modified)

Added `gldevices` to `target_link_libraries`.

### `src/abstract/Devices.h` (modified)

Silenced a `[[nodiscard]]` warning on the `EventManager::Instance()` call in the constructor by casting to `void`.

---

## Decisions and rationale

### Forward-declaring `GLFWwindow*` in the header
Keeps GLFW headers out of every TU that includes `GLDevices.h`. `glfw` is therefore a PRIVATE link dependency and consumers see only the `abstract::Devices` interface.

### Wayland disabled, X11 used
`wayland-scanner` was absent on the build machine. The option is explicit in CMakeLists so it can be re-enabled per-platform when the toolchain supports it.

### Static callbacks publish to `EventManager` directly
Since `EventManager` is a singleton, callbacks need no instance pointer. This avoids `glfwSetWindowUserPointer` boilerplate and keeps callbacks as simple one-liners.

### `GLFW_REPEAT` treated as `kKeyDown`
Held keys fire repeat events. Mapping these to `kKeyDown` lets game systems implement held-key logic (e.g., movement) without a separate event type, matching common engine conventions.

### Scroll Y-axis only
Horizontal scroll (`xoffset`) is discarded — the engine event model has one `wheel_delta` field and vertical scroll is the universally useful signal.

## Output to keep in mind

- GLFW is fetched with `GIT_TAG 3.4` and stored in `external/glfw-src/`.
- On Windows/macOS the `GLFW_BUILD_WAYLAND` option is irrelevant; the X11 flag may need adjustment.
- The `gldevices` library links `abstract` PUBLIC — anything linking `gldevices` also gets `abstract` and transitively `core`.
- `GLDevices::GetWindow()` returns `GLFWwindow*` as `void*`; cast with `static_cast<GLFWwindow*>(devices.GetWindow())` at the call site.

## Skills and guidance followed

- `src/CLAUDE.md`: one class per header/cpp, new modules need CMakeLists + src/CMakeLists entry
- Root `CLAUDE.md`: branch/PR workflow, history file, conventional commits
