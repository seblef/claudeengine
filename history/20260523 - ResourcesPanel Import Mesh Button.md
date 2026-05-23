# ResourcesPanel — Import Mesh shortcut button (issue #264)

## Changes

### `src/editor/ResourcesPanel.h`
- Added `SetOnImportMesh(std::function<void()>)` callback setter.
- Added private `on_import_mesh_` member.

### `src/editor/ResourcesPanel.cpp`
- Replaced the plain `TreeNodeEx` Meshes header with a formatted node
  (`"##mesh_header"`, `ICON_FA_CUBE Meshes`) plus an inline `SmallButton`
  (`ICON_FA_FILE_IMPORT`) positioned at `GetWindowWidth() - 30.f`.
- Button fires `on_import_mesh_` when set.

### `src/editor/EditorWindow.cpp`
- Wired `SetOnImportMesh` → `ImportMesh()` (existing NFD dialog).

## Decisions

- **Single button, offset -30**: only one button in the Meshes header (no "New Mesh"
  action exists), so offset matches the import-only layout from the issue spec.
- **Mirrors Material import pattern**: keeps the two section headers visually consistent.

## Output to keep in mind

- `ImportMesh()` in `EditorWindow` handles full NFD lifecycle; the callback keeps
  the panel decoupled from NFD.

## Skills / instructions used

- `impl-issue` skill
- `src/editor/CLAUDE.md` — one class per file, ImGui lifecycle rules
- `src/CLAUDE.md` — Google C++ style, include paths relative to `src/`
- CLAUDE.md git workflow
