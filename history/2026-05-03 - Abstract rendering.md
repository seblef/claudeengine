# Base abstract classes for rendering (Issue #19)

## Changes

### `src/abstract/Shader.h` (new)

Abstract shader program, inheriting `core::Resource<std::string>`.

| Member | Description |
|--------|-------------|
| `virtual void Activate() = 0` | Binds the program for subsequent draw calls |
| `protected Shader(const std::string& name)` | Registers the shader in the resource registry |

Concrete implementations compile/link GPU programs in their constructor, set `initialized_ = true` on success, and implement `Activate()`.

### `src/abstract/BufferUsage.h` (new)

GPU buffer access hint enum used by vertex (and future index/constant) buffers.

| Value | Meaning |
|-------|---------|
| `kDynamic` | Updated frequently (e.g., per-frame particle data) |
| `kImmutable` | Written once at creation, never updated |
| `kStaging` | CPU-writable; used to stage data before transferring to GPU memory |

### `src/abstract/VertexBuffer.h` (new)

Abstract vertex buffer holding a typed vertex array on the GPU.

| Member | Description |
|--------|-------------|
| `VertexBuffer(VertexType, num_vertices, BufferUsage)` | Stores layout parameters |
| `virtual void Bind() = 0` | Sets buffer as active vertex source |
| `virtual void Fill(data, num_vertices, offset=0) = 0` | Uploads vertex data; offset defaults to 0 |
| `GetVertexType()`, `GetNumVertices()`, `GetBufferUsage()` | Attribute getters |

---

## Decisions and rationale

### `Shader` ID is `std::string`
Shader names are the natural asset key in engine pipelines. `std::string` satisfies `std::map` key requirements and is immediately human-readable in logs and asset manifests.

### `VertexBuffer` is not a `Resource`
The issue specifies specific constructor parameters and getters but says nothing about a named registry or shared ownership. Vertex buffers are typically created per-mesh and destroyed with their owner — they are not looked up by name at runtime.

### `BufferUsage` in its own header
`src/CLAUDE.md` requires one type per header. `BufferUsage` is used by vertex buffers today and may be reused by index buffers, constant buffers, etc. tomorrow.

### `Fill` offset defaults to 0
Full-buffer uploads (offset = 0) are the overwhelmingly common case. The default eliminates boilerplate at every call site while keeping partial-update capability available.

## Output to keep in mind

- `Shader` ref count starts at 1 (inherited from `Resource`); callers must call `AddRef()`/`Release()`.
- `Shader::Activate()` is the only rendering contract — no uniforms, no attribute binding here; those belong in the concrete implementation.
- `VertexBuffer::Fill()` takes a raw `void*`; the caller is responsible for ensuring the data matches the declared `VertexType` and that `num_vertices + offset <= GetNumVertices()`.

## Skills and guidance followed

- `src/CLAUDE.md`: one class/enum per header, project-relative includes, Google C++ style
- Root `CLAUDE.md`: branch/PR workflow, history file, conventional commits
