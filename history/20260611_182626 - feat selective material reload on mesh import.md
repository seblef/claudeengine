# feat(editor): Selective material reload on mesh import

**Issue:** #472  
**Branch:** feat/selective-material-reload-on-mesh-import-472

## Problem

During mesh import (`MeshEditorWindow`), any user action that invalidated the
preview (scale change, rotation, material name edit) triggered a full rebuild via
`RebuildPreview()`. That function called `ClearPreview()` first, which released
all entries in `preview_mats_`. Because the preview held the **only** reference to
those materials (ref-count = 1), they were destroyed and removed from the global
`Resource` registry. The next `RebuildPreview()` call had to reload every material
slot from disk — including texture uploads — causing visible lag on every transform
adjustment.

## Root Cause

`core::Resource::Release()` deletes the object and erases it from the registry
when the ref-count reaches zero. `preview_mats_` was the sole owner of materials
loaded specifically for the preview, so `ClearPreview()` was effectively purging
the registry of those entries on every rebuild.

## Fix

Introduced a **per-session material cache** (`session_mat_cache_`) in
`MeshEditorWindow`:

- A `std::map<std::string, game::GameMaterial*>` that holds an **extra reference**
  to each material loaded during an import/edit session.
- Because the cache holds its own ref, `ClearPreview()` dropping the preview
  reference leaves the material alive in the registry (ref-count ≥ 1).
- On the next `RebuildPreview()`, `session_mat_cache_.find(mat_name)` hits →
  the existing material instance is reused with a simple `AddRef()`. No disk I/O,
  no texture upload.
- Materials with a **new or changed path** (first time they appear in the session)
  are loaded via `GameMaterial::GetOrLoad` as before, then stored in the cache.
- The cache is cleared (all refs released) at the start of a new session
  (`OpenImport`, `OpenEdit`) and in the destructor.

`ClearPreview()` is now also called explicitly at the top of `OpenImport` and
`OpenEdit` to ensure the previous session's preview refs are released before the
cache is wiped, preventing any dangling-reference window.

## Files Changed

- `src/editor/MeshEditorWindow.h` — added `#include <map>`, new private member
  `session_mat_cache_`, and `ClearSessionMaterialCache()` declaration.
- `src/editor/MeshEditorWindow.cpp` — implemented `ClearSessionMaterialCache()`;
  updated `~MeshEditorWindow`, `OpenImport`, `OpenEdit`, and `RebuildPreview`.

## Decisions & Rationale

- **Cache granularity: per-session, not per-name globally.** Materials loaded for
  preview are short-lived and session-scoped. Keeping them in a global cache would
  outlive the import dialog unnecessarily.
- **No geometry caching.** Transform changes require rebuilt geometry (vertex
  positions/normals change). Only the material reload was the bottleneck.
- **Procedural fallback materials (`__proc_*`) are not cached.** They are cheap
  to create (no disk I/O) and their names are index-based, so caching them
  would not save anything meaningful.

## Notes for Future Work

- If material reload latency is ever noticed elsewhere (e.g. map load), the same
  pattern (persistent ref in a caller-owned cache) can be applied.
- The `session_mat_cache_` grows monotonically within a session (stale names are
  not evicted). This is intentional: the overhead is negligible for the number of
  material slots a mesh can have.

## Skills / Instructions Used

- `src/editor/CLAUDE.md` — one class per file pair, editor is leaf of dependency graph
- `src/CLAUDE.md` — Google C++ style, include paths from `src/`
- Root `CLAUDE.md` — git workflow (feat/ branch, cpplint, conventional commits, PR to dev)
