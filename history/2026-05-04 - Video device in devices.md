# Video device as component of Devices class (Issue #25)

## Changes

### `src/abstract/Devices.h` (modified)

- Forward-declares `class VideoDevice`
- Adds `protected: VideoDevice* video_device_ = nullptr`
- Adds `[[nodiscard]] VideoDevice* GetVideoDevice() const`

### `src/gldevices/GLDevices.h` (modified)

- Adds `#include <memory>` and `class GLVideoDevice` forward declaration
- Adds `private: std::unique_ptr<GLVideoDevice> gl_video_device_`

### `src/gldevices/GLDevices.cpp` (modified)

After the GLFW window is created and the context is made current, constructs `GLVideoDevice` and stores it:

```cpp
gl_video_device_ = std::make_unique<GLVideoDevice>(width, height, !fullscreen, window_);
video_device_ = gl_video_device_.get();
```

### `src/app/main.cpp` (modified)

Removes the direct `GLVideoDevice` construction. Retrieves the video device via `devices.GetVideoDevice()` and uses it through the abstract interface.

---

## Decisions and rationale

### `GLDevices` owns `GLVideoDevice` via `unique_ptr`; `Devices` holds a raw `VideoDevice*`
The concrete subclass creates and owns the concrete video device. The base class exposes it through a non-owning raw pointer, keeping `abstract::Devices.h` free of any VideoDevice header include (only a forward declaration is needed).

### Forward-declaration in `Devices.h`, not full include
`Devices.h` only needs to return a `VideoDevice*` — a pointer to an incomplete type is sufficient. Including `VideoDevice.h` would pull in all its transitive headers into every TU that includes `Devices.h`.

### `video_device_` is `protected`
Concrete subclasses (GLDevices) must write to it. External callers access it only via the `GetVideoDevice()` getter.

### `GLVideoDevice` created after `glfwMakeContextCurrent`
OpenGL calls require an active context. By creating `GLVideoDevice` after the context is bound, `BeginFrame`/`ClearRenderTargets` are safe to call from the first frame.

## Output to keep in mind

- `GetVideoDevice()` returns a raw pointer; lifetime is tied to `GLDevices`. Do not store it past the lifetime of the `Devices` instance.
- The destruction order is correct: `gl_video_device_` (unique_ptr) is destroyed before `window_` because members are destroyed in reverse declaration order.

## Skills and guidance followed

- `src/CLAUDE.md`: one class per header, project-relative includes, Google C++ style
- Root `CLAUDE.md`: branch/PR workflow, history file, conventional commits
