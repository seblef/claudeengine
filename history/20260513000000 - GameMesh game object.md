# GameMesh game object

**Date:** 2026-05-13
**Issue:** #128
**PR:** #139
**Branch:** feat/game-mesh

## Changes

- `src/game/GameMesh.h` — declares `GameMesh`, a `GameObject` subclass owning a `MeshTemplate*` and a `std::unique_ptr<renderer::RenderableMeshInstance>`
- `src/game/GameMesh.cpp` — implements constructor/destructor (AddRef/Release), `OnAddedToScene`, `OnRemovedFromScene`, `OnWorldTransformUpdated`
- `src/game/CMakeLists.txt` — added `GameMesh.cpp` to the `game` STATIC library

## Decisions

**Instance lifetime tied to scene membership:** `RenderableMeshInstance` is created in `OnAddedToScene()` and destroyed in `OnRemovedFromScene()`. This keeps GPU resources (visibility system slots, per-submesh `MeshInstance` objects) allocated only while the object is actually in the scene, matching the renderer's ownership model.

**AddRef/Release on MeshTemplate:** The constructor calls `AddRef()` and the destructor calls `Release()` so the shared mesh geometry stays alive for the GameMesh's entire lifetime, regardless of how many other GameMesh objects reference it.

**Null guard in OnWorldTransformUpdated:** The instance pointer is checked before forwarding the transform, since `OnWorldTransformUpdated()` can be called before `OnAddedToScene()` (e.g., setting initial world position before adding to scene).

## Output to keep in mind

- `GameMesh` is the primary way to place a `MeshTemplate` in the world. Use `MeshTemplate::GetOrLoad()` to obtain the template, then pass it to `GameMesh`.
- `GameSystem::AddObject()` / `RemoveObject()` (issue #131) must call `OnAddedToScene()` / `OnRemovedFromScene()` to trigger instance creation/destruction.
- The `always_visible` flag bypasses frustum culling; default is `false` (octree-based culling).

## Skills / instructions used

- `src/CLAUDE.md`: include root is `src/`; one class per file; Google C++ style
- `src/game/CLAUDE.md`: dependency graph (game → renderer → abstract → core); key invariants for scene membership
- Root `CLAUDE.md`: git workflow, conventional commits, history file requirement
