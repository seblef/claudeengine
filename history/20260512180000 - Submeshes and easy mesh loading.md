# Submeshes and easy mesh loading

**Date:** 2026-05-12  
**Issue:** #122 — [feat] Submeshes and easy mesh loading  
**Branch:** feat/renderer-submesh-mesh-loader  
**Milestone:** Mesh Pipeline v1

---

## Summary

Adds two new renderer concepts — `RenderableMesh` and `RenderableMeshInstance` — that group multi-submesh models into a single scene object. Adds `MeshLoader`, a one-call bridge that detects format from extension, imports via the appropriate `mesh::` class, and returns a GPU-ready `RenderableMesh`. Updates the app entrypoint to use the new API.

---

## New files

### `src/renderer/RenderableMesh.h/.cpp`
Container that owns N submeshes, each with a `GeometryData` and a `Material`. Public API:

```cpp
void AddSubmesh(unique_ptr<GeometryData>, unique_ptr<Material>);
size_t          GetSubmeshCount() const;
Mesh*           GetSubmesh(size_t idx) const;
const BBox3&    GetLocalBBox() const;   // union of all submesh AABBs
```

The `AddSubmesh` call expands the combined local bbox from the geometry's bbox. Because `Mesh` stores raw pointers (no ownership), `RenderableMesh` holds both the `unique_ptr` storage and the lightweight `Mesh` wrapper so lifetimes are correct.

### `src/renderer/RenderableMeshInstance.h/.cpp`
A `Renderable` that holds a `RenderableMesh*` and a `vector<unique_ptr<MeshInstance>>` (one per submesh). World matrix is synced to each sub-instance at `Enqueue()` time, so callers need only call `SetWorldMatrix` on the parent:

```cpp
void Enqueue() override {
  for (auto& inst : sub_instances_) {
    inst->SetWorldMatrix(GetWorldMatrix());   // sync from parent
    MeshRenderer::Instance().AddInstance(inst.get());
  }
}
```

Sub-instances are not registered with any visibility system — culling is decided at the `RenderableMeshInstance` level using the combined AABB.

### `src/renderer/MeshLoader.h/.cpp`
Static utility that maps extension → importer/reader:

| Extension | Handler |
|---|---|
| `.obj` | `mesh::ObjImporter` |
| `.fbx` | `mesh::FbxImporter` |
| `.emesh` | `mesh::EmeshReader` |

The `mat_desc` parameter (default = `MaterialDesc{}`) applies the same material to all submeshes. A per-slot material system is deferred to a later milestone.

The index downcast (`uint32_t` → `uint16_t`) is handled inside `MeshLoader`, removing that responsibility from the call site.

---

## Modified files

### `src/renderer/CMakeLists.txt`
Added the three new `.cpp` files and added `mesh` as a PRIVATE link dependency so the renderer module can use importers without exposing them to callers.

### `CMakeLists.txt` (root)
Added `configure_file` to copy `blender_279_default_6100_ascii.fbx` from the ufbx test suite to `data/meshes/demo.fbx` at CMake configure time (no binary blob committed).

### `data/meshes/demo.obj` (new)
Hand-crafted unit cube: 8 positions, per-face normals, 4 UVs, 12 triangles.

### `src/app/main.cpp`
Replaced the manual `RegisterMeshData` lambda + four ownership vectors with two `MeshLoader::Load` calls and two `RenderableMeshInstance` objects. Also strips the old procedural scene (300 cubes, 300 spheres, 40 omni lights) in favour of 3 targeted lights around the two demo cubes.

---

## Decisions and rationale

| Decision | Rationale |
|---|---|
| `RenderableMesh` owns geometry and materials | Clean lifetime management: the model is a self-contained asset; callers don't need to juggle raw lifetimes. |
| World matrix sync at `Enqueue()` | Avoids needing a virtual `SetWorldMatrix` override. Static objects (world matrix set once) pay zero cost; dynamic objects pay one `SetWorldMatrix` call per submesh per frame — negligible. |
| Sub-instances not in visibility system | Only the `RenderableMeshInstance` level participates in culling. Per-submesh culling would require submesh-level octree nodes — not worth the complexity for now. |
| `mesh` as PRIVATE dep of `renderer` | `MeshLoader.h` exposes no `mesh::` types; callers include only renderer headers. |
| Default `MaterialDesc{}` parameter | Lets quick-load calls omit material specification; callers that need custom materials pass the descriptor. |
| `MeshLoader` in `renderer` | Needs both importer (mesh) and GPU creation (renderer); placing it in renderer avoids coupling the pure-CPU mesh module to GPU types. |

---

## Skills used
- `impl-issue`

## Instructions followed
- Google C++ Style Guide (cpplint clean, zero warnings).
- One class per `.h` / `.cpp`.
- `#include` paths project-relative from `src/`.
- Conventional commit message.
- History file in `history/`.
