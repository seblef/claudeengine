# Geometry Buffers

**Date:** 2026-05-04
**Branch:** feat/geometry-buffers
**Issue:** #36

## Summary

Added `GeometryBuffer` — an abstraction that combines a vertex buffer and an index buffer into a single managed object. The demo quad now uses a single `CreateGeometryBuffer` call instead of separate `CreateVertexBuffer` + `CreateIndexBuffer` calls.

Also added `SetIndexType` to `VideoDevice` (alongside `SetPrimitiveType`) so callers can declare the index element format as render state, independent of buffer binding.

## Changes

### New files
- `src/abstract/GeometryBuffer.h` — abstract class: constructor, `Bind()`, `FillVertices()`, `FillIndices()`, getters
- `src/gldevices/GLGeometryBuffer.h` — declaration of concrete class owning `GLVertexBuffer` + `GLIndexBuffer` by value
- `src/gldevices/GLGeometryBuffer.cpp` — implementation; constructor captures IBO in VAO

### Modified files

**`src/abstract/VideoDevice.h`**
- Added `SetIndexType(IndexType)` pure-virtual method to the render-state section
- Added `CreateGeometryBuffer(...)` pure-virtual factory

**`src/gldevices/GLVideoDevice.h` / `.cpp`**
- `SetIndexType` converts `IndexType` enum to `GL_UNSIGNED_SHORT` or `GL_UNSIGNED_INT` and stores it in `current_index_type_`
- `CreateGeometryBuffer` constructs `GLGeometryBuffer` and optionally fills vertex/index data

**`src/gldevices/CMakeLists.txt`**
Added `GLGeometryBuffer.cpp` to the static library sources.

**`src/app/main.cpp`**
- Replaced separate `vb` + `ib` + three-line bind sequence with a single `CreateGeometryBuffer` call
- Setup: `gb->Bind(); video->SetIndexType(kUInt16); video->SetPrimitiveType(kTriangleList)`
- Render loop: `gb->Bind(); video->RenderIndexed(6)`
- Removed includes for `IndexType.h` (pulled in transitively via `VideoDevice.h` → `GeometryBuffer.h`)

## Design Decisions

### No `BindGeometryBuffer` on VideoDevice
Render state (`SetPrimitiveType`, `SetIndexType`) is set explicitly by the caller. This keeps the device's state machine orthogonal to buffer binding — buffers are bound on the object (`gb->Bind()`), state is set on the device. `BindGeometryBuffer` would have mixed both concerns.

### `SetIndexType` as render state
Mirrors `SetPrimitiveType`. Both are draw-call parameters that the device remembers between calls, not per-buffer data. The caller decides when to change them.

### VAO captures IBO
`GLGeometryBuffer` constructor calls `vb_.Bind()` then `ib_.Bind()`. Since `GL_ELEMENT_ARRAY_BUFFER` is VAO state, the IBO binding is captured in the VAO. Subsequent `Bind()` calls only bind the VAO and get the IBO back for free.

### Members by value in `GLGeometryBuffer`
`vb_` and `ib_` are stored by value rather than as `unique_ptr`. This avoids a layer of indirection and heap allocation — the `GLGeometryBuffer` itself is already heap-allocated by `CreateGeometryBuffer`.

## Next Steps / Output to Remember

- `GeometryBuffer` supersedes separate `VertexBuffer` + `IndexBuffer` for typical mesh rendering. The lower-level classes remain available for advanced use (streaming, partial updates, etc.).
- `SetIndexType` must be called before `RenderIndexed` — the default is `GL_UNSIGNED_INT`. For `kUInt16` geometry buffers, calling `SetIndexType(kUInt16)` is mandatory.
- `BindIndexBuffer` still exists on `VideoDevice` (from issue #35). Future cleanup could remove it in favour of `SetIndexType` + `ib->Bind()` for the lower-level path.

## Skills and Instructions Used

- `impl-issue` skill
- `CLAUDE.md`: git workflow, conventional commits, history file, PR
- `src/CLAUDE.md`: one class per file, include from `src/`, Google C++ style
