# OpenGL Shaders (Issue #29)

## Changes

### `src/core/Color.h` (modified)

Added `Color::kGreen {0, 1, 0, 1}` and `Color::kBlue {0, 0, 1, 1}` following the existing `inline const Color Color::kXxx {…}` pattern.

### `src/gldevices/GLShader.h` (created)

Declares `gldevices::GLShader : public abstract::Shader`:
- Constructor loads and compiles `<name>_vs.glsl` / `<name>_ps.glsl` from the data folder
- `Activate()` — binds the linked program via `glUseProgram`
- `CompileStage()` — private static helper for single-stage compilation and error logging
- `program_id_` — the linked GL program handle

### `src/gldevices/GLShader.cpp` (created)

- Starts with `#define GL_GLEXT_PROTOTYPES` so that `<GL/gl.h>` pulls in `<GL/glext.h>` with shader function prototypes (same pattern as `GLVertexBuffer.cpp`)
- Source files read via a local `ReadFile()` helper using `std::ifstream` + `std::ostringstream`
- Paths: `Config::GetDataFolder() / "shaders" / "glsl" / (name + "_vs|ps.glsl")`
- On any failure (missing file, compile error, link error) GL resources are freed and `initialized_` stays `false`
- Destructor: `glDeleteProgram(program_id_)` (guarded by `if (program_id_)`)

### `src/gldevices/GLVideoDevice.cpp` (modified)

Implements `CreateShader()`:
1. `Resource<std::string>::Get(name)` — returns existing shader with incremented refcount if found
2. Otherwise: `new GLShader(name)` — constructor auto-registers in the resource map
3. If init failed: `Release()` (→ delete) and return `nullptr`
4. On success: return the newly created shader (ref_count=1, caller owns it)

### `src/gldevices/GLVideoDevice.h` (modified)

Updated doc comment for `CreateShader()` to reflect actual behaviour.

### `src/gldevices/CMakeLists.txt` (modified)

Added `GLShader.cpp` to the `gldevices` static library source list.

### `src/app/main.cpp` (modified)

Demo additions before the render loop:
- `CreateShader("passthrough_color_2d")` — loads and compiles the GLSL shader
- Six `Vertex2D` vertices forming a fullscreen quad with corner colors (Red, Blue, White, Green)
- `CreateVertexBuffer(k2D, 6, kImmutable, quad)` — uploads the quad data to GPU
- `shader->Release()` after the loop

### `data/shaders/glsl/passthrough_color_2d_vs.glsl` (created)

Vertex shader: passes clip-space position (`in_position.xy`) and vertex color (`in_color`) to the fragment stage. Takes attributes at locations 0/1/2 matching the `VertexBase` layout.

### `data/shaders/glsl/passthrough_color_2d_ps.glsl` (created)

Fragment shader: outputs the interpolated `v_color` unchanged.

---

## Decisions and rationale

### Shader files named `{name}_vs.glsl` / `{name}_ps.glsl`
Follows the convention in `data/shaders/glsl/CLAUDE.md`: both stages share the same base name with `_vs` / `_ps` suffixes. The `GLShader` constructor appends these suffixes automatically.

### `loguru.hpp` included directly in `GLShader.cpp`, not `core/Logger.h`
`core::Logger` is a static facade that wraps loguru initialisation. It does not re-export the `LOG_F` macro. Any TU that needs `LOG_F` must include `<loguru.hpp>` directly — the same pattern used in `GLDevices.cpp`.

### `GL_GLEXT_PROTOTYPES` before all includes
`<GL/gl.h>` includes `<GL/glext.h>` internally. The `glext.h` include guard prevents re-inclusion, so `GL_GLEXT_PROTOTYPES` must be defined before the first transitive pull of `glext.h`. Placing `#define GL_GLEXT_PROTOTYPES` as the first line of the `.cpp` (before the header that pulls `<GL/gl.h>`) guarantees this.

### `initialized_` set only on full pipeline success
If vertex compile, fragment compile, or link fails, `initialized_` stays `false`. `GLVideoDevice::CreateShader` checks this and calls `Release()` to destroy the failed shader before returning `nullptr`. This keeps the resource registry clean.

### No VAO / draw call in this issue
The rendering pipeline (VAO setup, attribute pointers, `glDrawArrays`) is deferred to the next issue. The demo validates shader loading and vertex buffer creation without rendering.

---

## Output to keep in mind

- `GLShader` requires an active GL context at construction and destruction.
- Shader name maps directly to file names: `"passthrough_color_2d"` → `passthrough_color_2d_vs.glsl` / `passthrough_color_2d_ps.glsl`.
- Attribute layout expected by shaders: location 0 = vec3 position, location 1 = vec4 color, location 2 = vec2 uv — this must match the VAO attribute setup in the next issue.
- `CreateShader` returns a raw pointer with ref_count=1; callers must call `Release()`.

## Skills and guidance followed

- `data/shaders/glsl/CLAUDE.md`: `_vs.glsl` / `_ps.glsl` naming convention
- `src/CLAUDE.md`: one class per header, project-relative includes, Google C++ style
- Root `CLAUDE.md`: branch/PR workflow, history file, conventional commits
