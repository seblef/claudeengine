# Abstract video device (Issue #22)

## Changes

### `src/abstract/VideoDevice.h` (new)

Abstract GPU backend class. Holds display parameters and declares all rendering contracts.

**Constructor**: `VideoDevice(int width, int height, bool windowed)`

| Method | Description |
|--------|-------------|
| `virtual void BeginFrame() = 0` | Prepares the backend at the start of each frame |
| `virtual void ClearRenderTargets(const core::Color&) = 0` | Clears render targets to a background color |
| `virtual void OnResize(int width, int height) = 0` | Handles window/screen resolution change |
| `virtual unique_ptr<VertexBuffer> CreateVertexBuffer(VertexType, n, usage, data=nullptr, offset=0) = 0` | Allocates a vertex buffer; optionally fills it immediately |
| `virtual Shader* CreateShader(const std::string& name) = 0` | Creates/retrieves a shader by name (ref_count=1) |
| `GetWidth()`, `GetHeight()`, `IsWindowed()` | Attribute getters |

---

## Decisions and rationale

### `CreateVertexBuffer` returns `std::unique_ptr<VertexBuffer>`
`VertexBuffer` is not a `Resource` — no ref-counting, no registry. `unique_ptr` expresses sole ownership clearly and prevents leaks.

### `CreateShader` returns `Shader*`
`Shader` inherits `Resource<std::string>`, which starts with ref_count=1. Returning a raw pointer is consistent with the ref-counting contract; wrapping in `unique_ptr` would interfere with `AddRef`/`Release`.

### Optional `data` and `offset` in `CreateVertexBuffer`
Most buffers are filled immediately after creation. Combining allocation and upload into one call avoids the common pattern of creating a buffer then immediately calling `Fill()`. `data=nullptr` means "allocate only".

### `protected` members (`width_`, `height_`, `windowed_`)
Concrete backends may need to update these (e.g., on resize). `protected` avoids an internal setter while keeping them inaccessible to external callers.

## Output to keep in mind

- `CreateShader` callers must call `Release()` when done with the pointer.
- `CreateVertexBuffer` with `data != nullptr` should internally call `Fill(data, num_vertices, offset)` after allocation.
- `OnResize` implementations should update `width_` and `height_` before any backend-specific resize work.

## Skills and guidance followed

- `src/CLAUDE.md`: one class per header, project-relative includes, Google C++ style
- Root `CLAUDE.md`: branch/PR workflow, history file, conventional commits
