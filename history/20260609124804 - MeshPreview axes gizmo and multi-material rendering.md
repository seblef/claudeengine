# MeshPreview — Axes Gizmo and Multi-Material Rendering

## Changes

### `renderer/PreviewRenderer` — axes gizmo

Added a world-space axes gizmo (X=red, Y=green, Z=blue, 1 unit each) rendered
after the composite pass into the output RTG.

**Implementation**:
- New member `axes_shader_` loads the existing `light_wireframe` shader (which
  reads `view_proj` from UBO slot 2 and outputs vertex color directly).
- New member `axes_vb_` is an immutable `kBase` vertex buffer holding 6
  `VertexBase` vertices (3 axes × 2 endpoints), created once at construction.
- New private method `DrawAxes(output_rtg)` binds the output RTG, disables
  depth test and depth writes, sets `kLineList` topology, activates the shader,
  binds the vertex buffer, calls `Render(6)`, then restores topology and
  culling.
- Called from `Render()` as step 5 after the composite pass. The preview scene
  CB (UBO slot 2) is still bound at that point, so the `light_wireframe` shader
  picks up the correct `view_proj` matrix without any extra work.

**Decisions**:
- Reused `light_wireframe` shader rather than writing a new one — it already
  does exactly what is needed (passthrough color + VP transform from UBO 2).
- Lines are drawn with depth testing disabled so they are always visible on top
  of the mesh, regardless of camera distance.
- Fixed vertex data is uploaded once at construction (`kImmutable` buffer) since
  the axes never change.

### `editor/MeshPreview` — Refresh() and multi-material

- Added `Refresh()` as an inline no-op on the public API. Material changes on a
  `MeshTemplate` propagate automatically: `MeshTemplate::SetMaterial(slot, mat)`
  calls `mesh_->SetMaterial(slot, mat)`, and the `MeshInstance` in `MeshPreview`
  holds a pointer to the same `Mesh` — so the next `Render()` call picks up the
  change without any explicit invalidation.
- Multi-material rendering was already functional: `MeshInstance` wraps a `Mesh`
  that may have multiple slots, and `MeshRenderer::Render()` already performs
  one draw call per submesh when `GetSubMeshCount() > 0`.

## Skills / CLAUDE.md rules applied

- `src/CLAUDE.md`: one class per `.h`/`.cpp`; Google C++ style; no new
  CLAUDE.md-prohibited patterns.
- `editor/CLAUDE.md`: editor is the leaf of the dependency graph; no new
  outbound dependencies added.
- `data/shaders/glsl/CLAUDE.md`: no new shaders needed — `light_wireframe`
  reused.

## Output for next features

- `PreviewRenderer` now always draws axes; if a future feature needs to toggle
  them off, add a `bool draw_axes_` member and a setter on `PreviewRenderer`.
- The 1-unit axes are in world space and may look very small for large meshes.
  If the import window needs scale-aware axes, expose a `SetAxesScale(float)`
  on `PreviewRenderer` (or scale by `bbox_diag_` in `MeshPreview`).
- `Refresh()` is a no-op today. If deferred destruction of `MeshTemplate`
  resources is introduced in the future (e.g. a frame-deferred release), it
  would be the right hook to recreate the `MeshInstance`.
