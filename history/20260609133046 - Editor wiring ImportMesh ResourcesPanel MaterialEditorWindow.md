# Editor Wiring — ImportMesh, ResourcesPanel, MaterialEditorWindow

**Issue**: #429
**Branch**: feat/editor-wiring-import-mesh-resources-material

## What changed

### `EditorWindow::ImportMesh()`

Previously, picking a `.emesh` file in the Import dialog opened `MeshEditorWindow`
in edit mode (same as right-clicking a mesh in the resource tree). The issue
requires that `.emesh` files bypass `MeshEditorWindow` entirely and load directly
into `EditorScene` — the same path used when the engine loads a mesh at game time.

Changed the `.emesh` branch to call `game::MeshTemplate::GetOrLoad(path, video_)`
and `scene_->AddMeshTemplate(tmpl)`, matching how `MeshEditorWindow::Commit`
registers the template after import.

`.fbx` / `.obj` files continue to open `MeshEditorWindow` in import mode.

### `editor/ImportedMaterialDesc.h` (new file)

The `ImportedMaterialDesc` struct was declared inline in `MaterialEditorWindow.h`.
It has been extracted to its own header per issue #429 requirements. A second
field `hint_paths` was added alongside `texture_paths`:

- `texture_paths[i]` — resolved path relative to `data/` (empty if not found)
- `hint_paths[i]`   — original unresolved name from the source file, shown dimly
                       in `MaterialEditorWindow` when `texture_paths[i]` is empty

### `MaterialEditorWindow`

- `hint_paths_` member added to store per-slot hint strings when the window is
  opened via `OpenWithDesc`.
- `Open()` clears `hint_paths_` so hints do not leak into regular material edits.
- `OpenWithDesc()` copies `desc.hint_paths` into `hint_paths_`.
- `RenderTextureSlot()` now shows the hint string (dimly, via `ImGuiCol_Text`
  pushed to `ImGuiCol_TextDisabled`) as the button label when no texture is set.
  Clicking the button still opens the native file dialog to pick a texture.

### `MeshEditorWindow::RenderMaterialSlots`

When building `ImportedMaterialDesc` for `MaterialEditorWindow::OpenWithDesc`,
now also fills `desc.hint_paths`: for each texture slot that is still unresolved
(empty `texture_paths[i]`), the slot's `original_name` is passed as the hint.

### `ResourcesPanel` / `EditorWindow` wiring (already present)

The context menu on mesh entries ("Edit…" → `on_mesh_edit_` callback) and its
wiring in `EditorWindow` (`SetOnMeshEdit` → `mesh_editor_->OpenEdit`) were
implemented in PR #428. No changes needed here.

## Decisions

- **Dim hint via button color** rather than a separate tooltip or extra table
  column: keeps the three-column texture table layout unchanged, and the
  clickable button still opens NFD on click.
- **Hint only for diffuse** in `MeshEditorWindow`: `ResolveAllTextures` only
  resolves the diffuse slot from the material name. All other slots get the same
  hint string so the user at least sees the original source name — they can
  resolve it manually via the file dialog.

## Skills / CLAUDE.md notes used

- `impl-issue` skill
- `src/CLAUDE.md` — one class per .h/.cpp, Google C++ style, include root is `src/`
- `src/editor/CLAUDE.md` — ImGui bracketing rules, no singletons in editor subsystems

## Output to keep in mind

- `ImportedMaterialDesc` is now in `editor/ImportedMaterialDesc.h`; any future
  header that uses the struct should include that file directly.
- `MeshEditorWindow` includes `MaterialEditorWindow.h` (for the pointer type),
  which transitively includes `ImportedMaterialDesc.h` — no extra include needed.
- `.emesh` picked via Import → directly in scene, no intermediate window.
  Right-click → Edit in ResourcesPanel → `MeshEditorWindow` in edit mode.
