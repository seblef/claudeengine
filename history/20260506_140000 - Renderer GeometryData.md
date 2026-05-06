# Renderer GeometryData

**Date:** 2026-05-06  
**Branch:** feat/renderer-geometry-data  
**Issue:** #60

---

## Summary

Added `renderer::GeometryData` — an immutable bundle of a `k3D` vertex buffer, a `kUInt16` index buffer, and the computed axis-aligned bounding box of the geometry. The cube demo in `main.cpp` now uses it instead of a raw `GeometryBuffer`.

---

## New Files

| File | Purpose |
|------|---------|
| `src/renderer/GeometryData.h` | Class declaration |
| `src/renderer/GeometryData.cpp` | Constructor (creates buffers, computes bbox) |

## Modified Files

| File | Change |
|------|--------|
| `src/renderer/CMakeLists.txt` | Added `GeometryData.cpp` to the renderer library |
| `src/app/main.cpp` | Replaced `CreateGeometryBuffer` with `renderer::GeometryData` |

---

## Key Decisions

### Immutability

No setters are provided. Vertex type (`k3D`), index type (`kUInt16`), buffer usage (`kImmutable`), and the bounding box are all fixed at construction time. This matches the intended use case: static geometry that never changes after upload.

### Constructor takes `num_triangles`, not `num_indices`

The interface mirrors the natural representation of mesh data (triangles), and `GetNumIndices()` derives from it (`num_triangles_ * 3`). This avoids passing an index count that must always be a multiple of 3.

### BBox computed from vertex positions

The bbox is seeded from `vertices[0].position` (both min and max), then extended via `BBox3::operator<<` for the remaining vertices. No extra allocation; the `BBox3::operator<<` introduced in issue #59 makes this straightforward.

### `Set()` binds vertex then index buffer

Follows the convention from `GeometryBuffer::Bind()` — vertex buffer bound first, then index buffer. The `abstract::VertexBuffer` and `abstract::IndexBuffer` are stored separately (as opposed to a `GeometryBuffer`) so that individual buffers can be accessed via the getters if needed.

---

## Output / Things to Keep in Mind

- `geo.GetNumIndices()` should be used in `RenderIndexed()` calls — avoids hardcoded counts.
- The bounding box is computed once at construction; it reflects the original rest-pose positions, not world-space positions after transformation.
- `abstract` links `core` with PUBLIC visibility, so `BBox3.h` and `Vertex3D.h` are reachable from `renderer` without adding a direct dependency.

---

## Skills / Instructions Applied

- `src/CLAUDE.md`: one class per `.h`/`.cpp`, include root is `src/`, Google C++ Style
- CLAUDE.md (root): history file in `history/`, conventional commits, PR targeting `dev`
