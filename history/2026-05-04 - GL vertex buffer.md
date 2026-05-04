# OpenGL Vertex Buffer (Issue #27)

## Changes

### `src/gldevices/GLVertexBuffer.h` (created)

- Declares `gldevices::GLVertexBuffer : public abstract::VertexBuffer`
- Holds a `GLuint vbo_` member (VBO handle)
- Overrides `Bind()` and `Fill(data, num_vertices, offset)`

### `src/gldevices/GLVertexBuffer.cpp` (created)

- Constructor: allocates a VBO via `glGenBuffers`, binds it, and calls `glBufferData` with a null pointer to pre-allocate GPU memory using the mapped usage hint
- Destructor: `glDeleteBuffers` — always runs, even if the context is destroyed before the object (GLFW guarantees the context outlives any object created by `GLDevices`)
- `Bind()`: `glBindBuffer(GL_ARRAY_BUFFER, vbo_)`
- `Fill()`: computes byte offset and byte count from `core::kVertexSize`, binds, then calls `glBufferSubData`
- `ToGLUsage()`: local anonymous-namespace helper mapping `BufferUsage` → GL draw hint

### `src/gldevices/GLVideoDevice.cpp` (modified)

- `CreateVertexBuffer()` now constructs a `GLVertexBuffer` and calls `Fill()` if `data` is non-null before returning

### `src/gldevices/GLVideoDevice.h` (modified)

- Updated doc comment for `CreateVertexBuffer()` to reflect real behaviour

### `src/gldevices/CMakeLists.txt` (modified)

- Added `GLVertexBuffer.cpp` to the `gldevices` static library source list

---

## Decisions and rationale

### `GL_GLEXT_PROTOTYPES` defined at the top of the `.cpp`, before any includes
`<GL/gl.h>` internally includes `<GL/glext.h>`. Because of `glext.h`'s include guard, a second include (after defining the macro) would be a no-op. Defining `GL_GLEXT_PROTOTYPES` before the first transitive pull of `<GL/glext.h>` is the only reliable way to get function prototypes declared in a single TU.

### `ToGLUsage` in an anonymous namespace (not a class method)
It is a pure mapping function with no need to touch class state. Keeping it local to the `.cpp` avoids polluting the header with `GLenum` and the `GL_*` constants.

### No VAO created here
Vertex Array Objects bind attribute layout to a specific shader program. Since no shader abstraction exists yet, binding attribute pointers is deferred. `Bind()` sets only the VBO — enough to upload data; draw calls will be added in a future issue.

### `glBufferData` called with `nullptr` on construction; `glBufferSubData` used in `Fill`
This separates allocation (fixed size, set once in the constructor) from upload (variable range, callable many times). It matches the contract in `abstract::VertexBuffer` where `num_vertices` is the capacity and `Fill` uploads a sub-range.

### Byte size via `core::kVertexSize`
The existing lookup table in `core/VertexType.h` already has a static assert that it covers all `VertexType` values, so no extra bounds check is needed.

---

## Output to keep in mind

- `GLVertexBuffer` requires an active GL context at construction, destruction, `Bind()`, and `Fill()`. It must not outlive the context created by `GLDevices`.
- `Fill()` binds the buffer internally; callers do not need to call `Bind()` first.
- No VAO is set up; draw calls still need attribute pointer configuration, which is a follow-up issue.
- Linking: `OpenGL::GL` is already in `gldevices/CMakeLists.txt`; `GL_GLEXT_PROTOTYPES` resolves function names against `libGL.so` at link time.

## Skills and guidance followed

- `src/CLAUDE.md`: one class per header, project-relative includes, Google C++ style
- Root `CLAUDE.md`: branch/PR workflow, history file, conventional commits
