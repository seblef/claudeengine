# WireframeRenderer — Issue #524

## Summary

Implemented `renderer::WireframeRenderer`, a unified singleton debug draw
renderer that replaces the three editor-side gizmo renderers
(`LightWireframeRenderer`, `ParticleGizmoRenderer`, `PlayerStartGizmoRenderer`)
with a single facility living inside the `renderer` module.  Because it lives in
`renderer`, objects like `Light` and `ParticleRenderable` can push wireframe
geometry during the visibility pass without importing the `editor` module and
violating the dependency graph.

## New files

| File | Role |
|------|------|
| `src/renderer/WireframeRenderer.h` | Singleton class declaration |
| `src/renderer/WireframeRenderer.cpp` | Full implementation |

`WireframeRenderer.cpp` was also added to `src/renderer/CMakeLists.txt`.

## Design decisions

### Two separate vertex lists (depth_list_ / overlay_list_)
Depth-tested and always-on-top geometry are kept in separate CPU vectors and
uploaded/drawn in two passes inside `Render()`.  This avoids any per-vertex
flag or sorting, keeping the draw path simple and the GPU state changes minimal.

### Single shared dynamic vertex buffer
One `abstract::VertexBuffer` is shared between the two passes and resized
geometrically (power-of-two growth, minimum 256) when the combined frame load
exceeds the current capacity.  The buffer is uploaded per-pass (depth then
overlay), so it must be at least as large as the larger of the two lists —
`EnsureCapacity` is called with the current pass size before each draw.

### Static private helpers
`AppendSegment`, `AppendCircle`, `AppendBox`, `AppendSphere`, `AppendCone`, and
`AppendLineList` are static: they take the target list by reference and perform
no per-frame heap allocation.  `TransformPoint` is a simple row-major 4×4
multiply for `w=1` points.

### Cone geometry
The cone points along local +Z with apex at the origin.  The base circle is
built directly in local space at `(0, 0, range)` by offsetting the XY plane
segments along Z, avoiding a secondary transform.  Four spokes connect the apex
to the ±X / ±Y rim points of the base circle.

### Shader name
Uses `video->CreateShader("wireframe")`, the shader renamed in issue #523.

### terrain_renderer_ is non-owned
`WireframeRenderer` stores a raw pointer to `terrain::TerrainRenderer` set via
`SetTerrainRenderer()`.  Ownership stays with whoever manages the terrain
(currently `Renderer`).  Passing `nullptr` safely detaches it.

### highlighted_key_ is an opaque pointer
The selection key is stored as `const void*` and compared by address only; it
is never dereferenced.  Callers pass their own object address and query
`IsHighlighted(ptr)` — no virtual dispatch or RTTI needed.

## State restored after Render()

`Render()` always restores depth test OFF, depth write ON, back-face culling,
and triangle-list primitive type after drawing, matching the convention of the
existing gizmo renderers.

## Output to keep in mind

- `BeginFrame()` must be called by `Renderer::Update()` at the start of each
  frame (before the visibility pass that pushes geometry) — this wiring is
  tracked in issue #522.
- The two old editor renderers (`LightWireframeRenderer`,
  `ParticleGizmoRenderer`, `PlayerStartGizmoRenderer`) are not yet removed;
  they will be deleted once their callers are migrated to `WireframeRenderer`.
- `PushLineList` connects adjacent pairs (i→i+1), not every pair — suitable for
  polylines, not arbitrary edge sets.

## Skills / guidelines used

- `CLAUDE.md` — one class per `.h/.cpp`, Google C++ style, include root `src/`.
- `src/renderer/CLAUDE.md` — (no file existed; followed `src/CLAUDE.md` rules).
- Singleton pattern matched `MeshRenderer` / `LightRenderer`.
- Power-of-two vertex buffer growth copied from `LightWireframeRenderer` /
  `ParticleGizmoRenderer`.
