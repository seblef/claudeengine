# Generic Object Renderer

**Date:** 2026-05-06
**Branch:** feat/object-renderer
**Issue:** #71

## Summary

Added `renderer::ObjectRenderer<T>`, a templated abstract batch renderer that sits between the `Renderable` base class (#69) and future concrete renderers (e.g. `MeshRenderer`, `ActorRenderer`). It owns a shader, maintains a per-frame instance queue, sorts it for optimal GPU state usage, and exposes a pure-virtual `Render()` hook for concrete draw call implementations.

## Changes

### New: `src/renderer/ObjectRenderer.h`

Header-only template class (template definitions must be visible at instantiation point — no `.cpp`).

**Template parameter:**
- `T` — a concrete renderable class; must provide `GetModel()` returning a pointer to a type that exposes `GetMaterial()`. Duck-typed via templates (no constraint syntax to preserve C++17 compatibility).

**Attributes (protected, accessible to subclasses):**
- `video_` (`abstract::VideoDevice*`) — non-owning; used for resource creation and draw commands
- `shader_` (`abstract::Shader*`) — owning; acquired via `CreateShader(name)` in constructor, released in destructor
- `instances_` (`std::vector<T*>`) — enqueued instances for the current frame; sorted by `PrepareRender()`, cleared by `EndRender()`

**Methods:**
- `ObjectRenderer(shader_name, video)` — loads shader by name; `video->CreateShader()` returns ref_count = 1
- `~ObjectRenderer()` — calls `shader_->Release()`
- `AddInstance(T*)` — appends to `instances_`
- `PrepareRender()` — `std::sort` by (material ptr, model ptr); groups instances that share textures and geometry, minimising GPU state switches
- `Render()` — pure virtual; concrete subclass iterates `instances_` and issues draw calls
- `EndRender()` — `instances_.clear()`; retains vector capacity for next frame

**No CMakeLists changes** — header-only; renderer's existing include path covers it.

## Decisions & Rationale

### Header-only template
C++ template method bodies must be visible at the instantiation site. Putting them in a separate `.cpp` would require explicit instantiations for each concrete T, which defeats the purpose. Header-only is the standard approach.

### `Render()` with no arguments, iterates `instances_` internally
The concrete subclass has protected access to `instances_` (already sorted). Accepting per-instance callbacks or passing the vector as a parameter would impose unnecessary coupling. The subclass calls its own draw logic in one virtual dispatch per frame.

### Sorting by pointer value
Material and model pointers are opaque, stable sort keys during a frame — pointer equality means "same GPU state". Sorting by `(GetMaterial(), GetModel())` pointer values guarantees all instances sharing textures are batched together, and within that group, instances sharing geometry are batched. This is the classic render-queue optimisation.

### No `Model` class defined here
`T` is duck-typed; no `Model` base class is required in this file. Future issues will define concrete model types (MeshModel, ActorModel, etc.) — those will be the types returned by `GetModel()`. Keeping `ObjectRenderer` generic avoids premature coupling.

### Shader ownership mirrors Material's texture pattern
`CreateShader(name)` starts the resource at ref_count = 1. The destructor calls `Release()`, which decrements and deletes when the count reaches 0. This is the same pattern used by `Material` for textures.

## Skills & Instructions Used

- `impl-issue` skill (CLAUDE.md git workflow)
- Root `CLAUDE.md`: conventional commits, history file, cpplint, PR to dev
- `src/CLAUDE.md`: one class per file, Google C++ style, include root is `src/`
