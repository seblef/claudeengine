# Base OpenGL video device (Issue #24)

## Changes

### `src/core/Color.h` (modified)

Added `kRed` constant: `{1, 0, 0, 1}`.

### `src/gldevices/GLVideoDevice.h` (new)

Concrete `VideoDevice` backed by OpenGL. Forward-declares `GLFWwindow` to keep GLFW headers private.

### `src/gldevices/GLVideoDevice.cpp` (new)

| Method | Implementation |
|--------|---------------|
| `GLVideoDevice(w, h, windowed, window)` | Delegates to `VideoDevice(w, h, windowed)`, stores `window_` |
| `BeginFrame()` | `glfwSwapBuffers(window_)` then `glViewport(0, 0, w, h)` |
| `ClearRenderTargets(color)` | `glClearColor` + `glClear(COLOR | DEPTH)` |
| `OnResize(w, h)` | Updates `width_`, `height_`, calls `glViewport` |
| `CreateVertexBuffer(...)` | Returns `nullptr` (not yet implemented) |
| `CreateShader(...)` | Returns `nullptr` (not yet implemented) |

### `src/gldevices/CMakeLists.txt` (modified)

- Added `GLVideoDevice.cpp` to the source list
- Added `find_package(OpenGL REQUIRED)` and linked `OpenGL::GL` as a PRIVATE dependency (OpenGL symbols are not exposed to consumers of `gldevices`)

### `src/app/main.cpp` (modified)

Creates `GLVideoDevice(640, 480, true, window)` after `GLDevices`. In the game loop, calls `video.BeginFrame()` and `video.ClearRenderTargets(core::Color::kRed)` before processing events.

---

## Decisions and rationale

### `BeginFrame` swaps the buffer
`glfwSwapBuffers` presents the previous frame before the new frame is prepared. This is the double-buffered "begin frame = flip + reset" pattern. On the first frame the window shows the default (black) for one iteration, then red on every subsequent frame.

### `OpenGL::GL` is PRIVATE
Only `GLVideoDevice.cpp` calls GL functions. Consumers of the `gldevices` target (e.g., `app`) do not need GL symbols in their own TUs.

### `struct GLFWwindow` forward-declared in main.cpp
`GetWindow()` returns `void*`; the cast to `GLFWwindow*` requires the type to be in scope. A forward declaration avoids pulling in all of GLFW into the app TU.

## Output to keep in mind

- The swap/clear ordering means there is a one-frame latency before the clear colour is visible.
- `CreateVertexBuffer` and `CreateShader` currently return `nullptr` — calling code must null-check.
- When `OnResize` is wired to the GLFW resize callback, it will also need to call `glfwSwapBuffers` or at minimum re-render to avoid a blank frame during resize on some platforms.

## Skills and guidance followed

- `src/CLAUDE.md`: one class per header/cpp, new modules need CMakeLists + src/CMakeLists entry
- Root `CLAUDE.md`: branch/PR workflow, history file, conventional commits
