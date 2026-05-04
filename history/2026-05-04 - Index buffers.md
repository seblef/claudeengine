# Index Buffers

**Date:** 2026-05-04
**Branch:** feat/index-buffers
**Issue:** #35

## Summary

Added full index buffer support: type enum, abstract class, OpenGL concrete class, factory and bind methods on VideoDevice, and an indexed draw call. The demo quad was refactored from 6 duplicated vertices to 4 unique vertices + 6 uint16 indices.

## Changes

### New files
- `src/abstract/IndexType.h` — `abstract::IndexType` enum (`kUInt16`, `kUInt32`)
- `src/abstract/IndexBuffer.h` — abstract `IndexBuffer` class mirroring `VertexBuffer`
- `src/gldevices/GLIndexBuffer.h` — declaration of `GLIndexBuffer`
- `src/gldevices/GLIndexBuffer.cpp` — IBO-backed implementation

### Modified files

**`src/abstract/VideoDevice.h`**
Three new pure-virtual methods:
- `BindIndexBuffer(IndexBuffer*)` — binds the IBO and tracks its type for `RenderIndexed`
- `CreateIndexBuffer(type, count, usage, data=nullptr, offset=0)` — factory returning `unique_ptr<IndexBuffer>`
- `RenderIndexed(num_indices, first_index=0, vertex_offset=0)` — indexed draw call

**`src/gldevices/GLVideoDevice.h` / `.cpp`**
- Implementations of the three new methods
- `current_index_type_` private member stores the current GL enum (default `GL_UNSIGNED_INT`)
- `BindIndexBuffer` converts `IndexType` to GL enum, stores it, then calls `ib->Bind()`
- `RenderIndexed` uses `glDrawElementsBaseVertex` (GL 3.2+) for `vertex_offset` support
- `CreateIndexBuffer` constructs `GLIndexBuffer` and optionally fills it

**`src/gldevices/CMakeLists.txt`**
Added `GLIndexBuffer.cpp` to the static library sources.

**`src/app/main.cpp`**
- Quad reduced from 6 vertices (with duplicates) to 4 unique vertices + 6 `uint16_t` indices
- IBO and VAO set up once before the render loop (VAO captures IBO binding)
- Render loop uses `video->RenderIndexed(6)` instead of `video->Render(6)`

## Design Decisions

### BindIndexBuffer on VideoDevice instead of IndexBuffer::Bind() alone
`glDrawElementsBaseVertex` requires knowing the element type (`GL_UNSIGNED_SHORT` or `GL_UNSIGNED_INT`) at draw time, but the draw call is on `VideoDevice` which has no reference to the current IBO. Three options were considered:

1. Add `BindIndexBuffer(IndexBuffer*)` to `VideoDevice` (chosen): Device tracks type; `IndexBuffer::Bind()` is still available for low-level access but typical callers go through the device. Consistent with D3D12's `IASetIndexBuffer` pattern.
2. Pass `IndexType` as a parameter to `RenderIndexed`: deviates from the issue's specified signature.
3. Let `IndexBuffer::Bind()` update device state: circular dependency.

Option 1 was chosen as cleanest and most consistent with how both backends (GL/DX12) work.

### glDrawElementsBaseVertex for vertex_offset
The standard `glDrawElements` does not support a vertex base offset. `glDrawElementsBaseVertex` (GL 3.2 core, available in GL 4.6) takes a `basevertex` that is added to each index before fetching, allowing multiple meshes packed into one VBO to share a single IBO. This matches the `vertex_offset` semantics in the issue.

### IBO binding captured in VAO
In OpenGL, `GL_ELEMENT_ARRAY_BUFFER` is part of VAO state. By calling `BindIndexBuffer` while the VAO is bound (after `vb->Bind()`), the IBO association is stored in the VAO. On subsequent frames, binding the VAO alone is sufficient — the IBO is automatically restored. The demo sets this up once before the loop and only rebinds the VAO per frame.

### uint16_t indices for the demo quad
The quad has 4 vertices, well within the 16-bit range. `kUInt16` saves 2 bytes per index (12 bytes total for 6 indices vs 24 bytes for uint32), and demonstrates the `kUInt16` code path.

## Next Steps / Output to Remember

- `VideoDevice::BindIndexBuffer()` must be called **while the VAO is active** (after `vb->Bind()`) for the IBO to be stored in the VAO state. If called before `vb->Bind()`, the IBO binding won't be captured and `glDrawElements` will fail.
- `current_index_type_` in `GLVideoDevice` defaults to `GL_UNSIGNED_INT`. A call to `BindIndexBuffer` before `RenderIndexed` is mandatory to get a correct type.
- `glDrawElementsBaseVertex` is not available in OpenGL ES. If ES support is ever added, an alternative codepath for `vertex_offset == 0` will be needed.
- The `GLIndexBuffer` does not own a VAO — the VBO/VAO owns it. This is consistent with OpenGL semantics (IBO binding is VAO state, not an independent object).

## Skills and Instructions Used

- `impl-issue` skill
- `CLAUDE.md`: git workflow (feat/ prefix, conventional commits, history file, PR)
- `src/CLAUDE.md`: one class per file, include from `src/`, Google C++ style
