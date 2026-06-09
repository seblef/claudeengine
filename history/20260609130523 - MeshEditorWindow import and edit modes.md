# MeshEditorWindow — import and edit modes

## Summary

Implements `MeshEditorWindow`, a floating ImGui window that replaces the previous
`EditorWindow::ImportMesh()` direct-load flow. The window handles both first-time
import (`.fbx` / `.obj`) and iterative editing of existing `.emesh` files.

## Files created

- `src/editor/MeshEditorWindow.h`
- `src/editor/MeshEditorWindow.cpp`

## Files modified

- `src/editor/MaterialEditorWindow.h` — added `ImportedMaterialDesc` struct and
  `OpenWithDesc(ImportedMaterialDesc)` overload; added `on_saved_` callback and
  `desc_material_owned_` lifetime flag.
- `src/editor/MaterialEditorWindow.cpp` — implemented `OpenWithDesc`; wired
  `on_saved_` callback into `Save()`; added `desc_material_owned_` release logic
  in `Open()`, `Render()`, and destructor.
- `src/editor/ResourcesPanel.h` — added `SetOnMeshEdit` callback.
- `src/editor/ResourcesPanel.cpp` — added right-click context menu with "Edit…" item
  for mesh template leaves.
- `src/editor/EditorWindow.h` — added `mesh_editor_` member (`unique_ptr<MeshEditorWindow>`).
- `src/editor/EditorWindow.cpp` — constructed `mesh_editor_`; wired `on_mesh_edit_`
  callback from `ResourcesPanel`; rewrote `ImportMesh()` to open `MeshEditorWindow`
  for `.fbx`/`.obj`/`.emesh`; called `mesh_editor_->Render()` every frame.
- `src/editor/CMakeLists.txt` — added `MeshEditorWindow.cpp`; linked `mesh` to editor.

## Design decisions

### Two-mode window

`MeshEditorWindow` operates in two modes selected at open time:

- **Import** (`OpenImport(path)`) — loaded via FBX/OBJ importer; scale pre-filled
  from `MeshData::unit_meters`; `[Import]` bakes scale × rotation and writes a new
  `.emesh` file, then adds the resulting `MeshTemplate` to the scene.
- **Edit** (`OpenEdit(path)`) — reads an existing `.emesh`; `[Save]` overwrites the
  file.  Scene reloading is handled implicitly via the `MeshTemplate` registry (the
  template is already in the scene before the editor was opened).

### Transform controls

Scale is a float input. Rotation is accumulated via six 90° buttons (−X +X −Y +Y −Z
+Z); each click pre-multiplies `rotation_` so transforms compose correctly. The
preview is rebuilt whenever scale or rotation changes.

### Preview rebuild

Applying the transform to vertices every frame would be wasteful. A `preview_dirty_`
flag is set whenever scale or rotation changes; `RebuildPreview()` is called once per
dirty frame before rendering. `RebuildPreview()` constructs a fresh procedural
`MeshTemplate` with per-slot `GameMaterial`s so the preview reflects saved material
assignments.

### Material slots

Each `SubMeshRange` becomes one row in the Materials table. `[Edit]` opens
`MaterialEditorWindow::OpenWithDesc(ImportedMaterialDesc)` with the slot name and any
textures found by `ResolveTexture()`. When the user saves in the material editor,
`on_saved` fires and updates `mat_slots_[i].saved_material_path`; the preview is
marked dirty so the next frame picks up the new material.

### `ImportedMaterialDesc` and `OpenWithDesc`

`ImportedMaterialDesc` carries:
- `slot_name`: key for the new `GameMaterial`.
- `texture_paths`: per-`TextureSlot` paths resolved against `data/textures/`.
- `on_saved`: callback fired with the saved material name.

`OpenWithDesc` creates a fresh `GameMaterial` (ref-counted, not backed by a file on
disk yet). `MaterialEditorWindow` tracks ownership via `desc_material_owned_` and
releases the material when the window is closed or another material is opened. This
prevents both use-after-free and leaks.

### Texture resolution

`ResolveTexture(name)` recursively scans `data/textures/` for a regular file whose
stem matches the material name. Returns a path relative to `data/` (e.g.
`textures/vehicles/body.png`). If unresolved, the diffuse hint is left empty and
the `MaterialEditorWindow` starts with no textures pre-filled.

### CMake: `mesh` linked to `editor`

The `mesh` module was previously only linked privately from `renderer`. The editor now
uses `FbxImporter`, `ObjImporter`, `EmeshReader`, and `EmeshWriter` directly, so
`mesh` is added to the editor's public link dependencies.

## Output to keep in mind

- `ImportedMaterialDesc` and `OpenWithDesc` implement the #429 spec; any future
  material-import workflow should reuse these.
- `MeshEditorWindow::ResolveTexture()` does a naive filename-stem search. A richer
  version could use FBX material texture paths (accessible via `ufbx_material`'s
  texture maps) — this would require extending `FbxImporter` or `MeshData`.
- The scene does not receive an `AddMeshTemplate` call in edit mode; the caller is
  responsible for having already registered the template. If the emesh is rewritten
  on disk, in-memory geometry will be stale until the app restarts. A future
  improvement could reload the `GeometryData` in-place.
- `cpplint` passes with no warnings.
- Skills used: `impl-issue`.
- Key guidelines followed: one class per `.h`/`.cpp`; Google C++ style; no
  premature abstractions; `cppcheck-suppress unusedStructMember` on all non-trivial
  struct fields; `CLAUDE.md` git workflow.
