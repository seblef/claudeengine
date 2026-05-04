# Refactor Index Buffer Binding

**Date:** 2026-05-04
**Branch:** fix/refactor-index-buffer-binding
**Issue:** #41
**Based on:** feat/index-buffers (PR #39)

## Summary

Removed `BindIndexBuffer` from `VideoDevice` and replaced it with `SetIndexType`, decoupling buffer binding from the video device. Index buffers are now bound directly via `IndexBuffer::Bind()`, and the device only tracks the element type for draw calls — consistent with how `SetPrimitiveType` works.

## Changes

### `src/abstract/VideoDevice.h`
- Removed `virtual void BindIndexBuffer(IndexBuffer* ib) = 0`
- Added `virtual void SetIndexType(IndexType type) = 0`

### `src/gldevices/GLVideoDevice.h`
- Removed `void BindIndexBuffer(abstract::IndexBuffer* ib) override`
- Added `void SetIndexType(abstract::IndexType type) override`

### `src/gldevices/GLVideoDevice.cpp`
- Removed `BindIndexBuffer` (which converted type and called `ib->Bind()`)
- Added `SetIndexType` (only the type conversion; `Bind()` is the caller's responsibility)

### `src/app/main.cpp`
Before:
```cpp
vb->Bind();
video->BindIndexBuffer(ib.get());
```
After:
```cpp
vb->Bind();
ib->Bind();
video->SetIndexType(abstract::IndexType::kUInt16);
```

## Design Rationale

`BindIndexBuffer` did two things: bound the IBO and tracked its type. This violates the single-responsibility principle and breaks the pattern established by vertex buffers, whose `Bind()` is called directly without going through `VideoDevice`. The refactoring makes binding symmetrical: both vertex and index buffers are bound on the object, and the device is only told about state that affects its draw calls (topology and index type). This is also more correct for multi-buffer scenarios where you may want to rebind buffers independently of setting draw-call state.

## Skills and Instructions Used

- `impl-issue` skill
- `CLAUDE.md`: git workflow, conventional commits, history file
- `src/CLAUDE.md`: Google C++ style guide
