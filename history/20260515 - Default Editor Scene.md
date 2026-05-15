# Default Editor Scene: Floor, Cube, and Global Light

**Date:** 2026-05-15
**Issue:** #170
**Branch:** feat/default-editor-scene
**Skills used:** impl-issue

---

## Summary

Two-part change: adds a `CreateCubeMesh()` primitive to the renderer, then populates the default editor scene with a floor plane, a cube, and a directional light.

---

## Part 1 — `renderer::CreateCubeMesh()`

### Geometry

24 vertices (4 per face) so each face carries its own normal, binormal, tangent, and UV. Sharing vertices across faces would require averaging normals at corners, which breaks per-face lighting.

UV range is 0→1 on each face. Per-face tangent space follows the issue spec:

| Face | Normal | Tangent | Binormal |
|------|--------|---------|----------|
| +Y (top) | (0,1,0) | (1,0,0) | (0,0,1) |
| -Y (bottom) | (0,-1,0) | (-1,0,0) | (0,0,1) |
| +X (right) | (1,0,0) | (0,0,-1) | (0,1,0) |
| -X (left) | (-1,0,0) | (0,0,1) | (0,1,0) |
| +Z (front) | (0,0,1) | (1,0,0) | (0,1,0) |
| -Z (back) | (0,0,-1) | (-1,0,0) | (0,1,0) |

### Winding order

The cross product analysis (n = (v_b−v_a)×(v_c−v_a)) showed two winding conventions are needed:
- **+Y and −Y (horizontal faces)**: index order `(b,b+2,b+1, b,b+3,b+2)` — same as `CreatePlaneMesh`.
- **+X, −X, +Z, −Z (vertical faces)**: index order `(b,b+1,b+2, b,b+2,b+3)`.

This gives outward-pointing geometric normals for every face, ensuring correct backface culling in the shadow pass.

### Header comment fix

The misleading comment "Only the position field is populated" has been updated to clarify that `CreatePlaneMesh` and `CreateCubeMesh` populate all fields.

---

## Part 2 — `EditorScene`

### Design

`EditorScene` owns the three game objects via `unique_ptr<>` and holds a non-owning `std::vector<GameObject*>` as the public object list. It registers/unregisters with `game::GameSystem::AddObject()`/`RemoveObject()` so that `OnAddedToScene()` and `OnRemovedFromScene()` are called correctly, registering renderables and lights with the renderer's visibility and light systems.

### Default objects

- **Floor**: `CreatePlaneMesh(video, 60.f, plane_mat)` — neutral grey (0.5, 0.5, 0.5), `always_visible=true` (large enough that it should never be culled by the octree frustum test).
- **Cube**: `CreateCubeMesh(video, cube_mat)` — light grey (0.7, 0.7, 0.7), scaled ×2, translated to (0,1,0). At scale 2 the cube extends ±1 unit, so with translation (0,1,0) its bottom face sits exactly on the floor at Y=0.
- **Light**: `GameLight(kGlobal, desc)` — warm sun-like colour (0.9, 0.85, 0.7), intensity 1.2, direction (-0.4, -0.8, -0.3).Normalized() — coming from the upper left.

### `MeshTemplate` for procedural meshes

`MeshTemplate` is ref-counted (`core::Resource`, starts at 1). The pattern for procedural meshes:
```cpp
auto* tmpl = new MeshTemplate(rmesh_ptr);  // ref_count = 1
auto* mesh = new GameMesh(tmpl);            // AddRef → ref_count = 2
tmpl->Release();                            // ref_count = 1; GameMesh destructor will bring to 0
```

### Ownership chain

```
EditorWindow owns EditorScene (unique_ptr)
  EditorScene owns floor_, cube_, light_ (unique_ptr)
  EditorScene::GetObjects() returns non-owning vector
  EditorViewport holds EditorScene* (non-owning, via SetScene())
```

`EditorWindow` declares `scene_` before `viewport_` so it's created first (EditorViewport needs it) and destroyed after (reverse declaration order).

### `GameSystem` in editor_app

`editor_app/main.cpp` now creates `game::GameSystem(&devices)` before `EditorSystem` and shuts it down after. `GameSystem::Update()` is never called from the editor — only `AddObject()`/`RemoveObject()` are used for object lifecycle. The editor drives its own render loop via `EditorViewport::Render()` → `Renderer::Update()`.

---

## Notes for future work

- Issue #175 (Resources panel) and #176 (Objects panel) will read `EditorScene::GetObjects()` to populate their trees.
- Issue #172 (object selection) will call `EditorScene::SetSelectedObject()` and `EditorViewport::SetSelectedObject()`.
- The floor uses `always_visible=true` because its world bbox (120×120) would be correctly included in the frustum but the octree node-splitting heuristics might place it poorly. Re-evaluate when the octree tuning is done.
