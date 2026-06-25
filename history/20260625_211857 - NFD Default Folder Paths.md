# NFD Default Folder Paths

**Issue**: #804  
**PR**: #808  
**Branch**: `fix/nfd-default-paths`

## Problem

All 11 NFD file dialogs that previously passed `nullptr` as `defaultPath` caused
the GTK file chooser to open in "Recent files" or text-entry mode, suppressing the
breadcrumb path bar (e.g. `home > seb > dev > claudeengine > data > meshes`). GTK
only renders breadcrumbs when the dialog is anchored to a real filesystem path.

## Changes

Three files were modified — only the `defaultPath` argument at each NFD call site:

| File | Method | Old | New |
|---|---|---|---|
| `EditorWindow.cpp` | `SaveAs` | `nullptr` | `data/maps` |
| `EditorWindow.cpp` | `LoadFromFile` | `nullptr` | `data/maps` |
| `EditorWindow.cpp` | `ImportMaterial` | `nullptr` | `data/materials` |
| `EditorWindow.cpp` | `ImportMesh` | `nullptr` | `data/meshes` |
| `TerrainEditorPanel.cpp` | Import PNG | `nullptr` | `data/textures` |
| `TerrainEditorPanel.cpp` | Import HDR/EXR | `nullptr` | `data/textures` |
| `TerrainEditorPanel.cpp` | Export PNG | `nullptr` | `data/textures` |
| `TerrainEditorPanel.cpp` | Export R16 | `nullptr` | `data/textures` |
| `VehicleEditorWindow.cpp` | `PickBodyMesh` | `nullptr` | `data/meshes` |
| `VehicleEditorWindow.cpp` | `PickFrontWheelMesh` | `nullptr` | `data/meshes` |
| `VehicleEditorWindow.cpp` | `PickRearWheelMesh` | `nullptr` | `data/meshes` |

## Decision rationale

- Path is computed locally per call site (`const std::string dir = (...).string()`)
  rather than cached, matching the existing pattern in `SoundEditorWindow` and
  `MaterialEditorWindow`. `GetDataFolder()` is cheap and call sites are rare.
- No new helper was introduced — the pattern is already established and three
  similar lines do not warrant an abstraction.

## Output / notes for next features

- The "already correct" sites (`MaterialEditorWindow`, `EnvironmentEditorPanel`,
  `ParticleEditorWindow`, `SoundEditorWindow`, `TerrainEditorPanel` lines 274/963)
  need no changes.
- If a new NFD dialog is added, always pass
  `(core::Config::GetDataFolder() / "<subfolder>").string().c_str()` — never `nullptr`.

## Skills / instructions used

- `impl-issue` skill
- `src/editor/CLAUDE.md` — editor module dependency graph and guidelines
- Root `CLAUDE.md` — git workflow and conventional commits requirements
