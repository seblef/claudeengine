# GameMaterial editor integration

**GitHub issue**: #206  
**Branch**: feat/game-material-editor  
**Milestone**: Game materials

## Summary

Replaced raw `renderer::Material*` references in the editor with `game::GameMaterial`, wired double-click on a material in the Resources panel to open the material editor window, and added a stub `MaterialEditorWindow` class (full UI in Issue #208).

## New / changed files

- `src/editor/EditorScene.h` — removed `materials_` map, `GetMaterials()`, `AddMaterial()`; added `game_materials_` list and `AddGameMaterial()`
- `src/editor/EditorScene.cpp` — removed `materials_["..."] = ...` lines; added material release loop in destructor
- `src/editor/ResourcesPanel.h` — removed `EditorScene` dependency; added `SetOnMaterialOpen()` callback setter
- `src/editor/ResourcesPanel.cpp` — reads `GameMaterial::GetRegistry()` instead of `scene.GetMaterials()`; double-click triggers `on_material_open_`; `Render()` no longer takes `scene` param
- `src/editor/MaterialEditorWindow.h` — new stub class with `Open(GameMaterial*)` and `Render()`
- `src/editor/MaterialEditorWindow.cpp` — stub implementation: shows titled ImGui window when a material is open
- `src/editor/EditorWindow.h` — added `material_editor_` member (forward-declared)
- `src/editor/EditorWindow.cpp` — wires callback, calls `material_editor_->Render()`, updates `ImportMaterial()` to use `GameMaterial::GetOrLoad`
- `src/editor/CMakeLists.txt` — added `MaterialEditorWindow.cpp`

## Changes

### `src/editor/EditorScene.h` / `EditorScene.cpp`

**Removed:**
- `std::map<std::string, renderer::Material*> materials_` private member.
- `GetMaterials() const` accessor.
- `AddMaterial(name, mat)` mutator.
- `namespace renderer { class Material; }` forward declaration.

**Added:**
- `#include "game/GameMaterial.h"` (needed for `AddGameMaterial` inline method).
- `std::vector<game::GameMaterial*> game_materials_` — holds refs to imported materials for lifetime management.
- `void AddGameMaterial(game::GameMaterial* mat)` — stores imported material ref; released on `EditorScene` destruction.
- Destructor now releases each `game_materials_` entry.

The floor/cube `GameMaterial` instances remain procedural (keyed `"__proc_editor_floor"` / `"__proc_editor_cube"`). They are excluded from the Resources panel via the `"__proc_"` filter in `ResourcesPanel`.

### `src/editor/ResourcesPanel.h` / `ResourcesPanel.cpp`

**Changed:**
- `Render(const EditorScene& scene)` → `Render()` — both materials and meshes now come from static registries; the scene parameter was unused.
- Materials now iterated from `game::GameMaterial::GetRegistry()` with `"__proc_"` entries skipped.
- Added `std::function<void(game::GameMaterial*)> on_material_open_` and `SetOnMaterialOpen()` setter.
- Double-click on a material leaf node invokes `on_material_open_`.

### `src/editor/MaterialEditorWindow.h` / `MaterialEditorWindow.cpp`

New stub class. `Open(GameMaterial*)` sets the active material and shows the window. `Render()` draws a titled ImGui window with a placeholder text. Full editing UI deferred to Issue #208.

### `src/editor/EditorWindow.h` / `EditorWindow.cpp`

- Added `std::unique_ptr<MaterialEditorWindow> material_editor_` member.
- Constructor wires `resources_panel_->SetOnMaterialOpen([this](auto* mat){ material_editor_->Open(mat); })`.
- `Render()` calls `material_editor_->Render()` after all docked panels, before the status bar.
- `ImportMaterial()`: replaced `new renderer::Material(out_path, video_)` + `scene_->AddMaterial()` with `game::GameMaterial::GetOrLoad(name, video_)` + `scene_->AddGameMaterial()`.

## Decisions

- **`ResourcesPanel::Render()` drops scene parameter**: meshes were already read from `MeshTemplate::GetAll()` (static), and materials now come from `GameMaterial::GetRegistry()` (also static). The `EditorScene` parameter was solely used for `GetMaterials()` — removing it simplifies the call sites.
- **`"__proc_"` filter in ResourcesPanel**: procedural scene materials (floor, cube) are internal and should not appear in the resources panel. The same convention used in `MeshTemplate::GetAll()` is applied here.
- **`game_materials_` lifecycle in `EditorScene`**: the `GetOrLoad` ref is held by `EditorScene`; on destruction it calls `Release()` on each. This mirrors how `mesh_templates_` is managed.
- **`MaterialEditorWindow` as a stub**: Issue #208 specifies the full editing UI (mesh preview, texture/color slots, save, apply-to-selection). The stub establishes the class boundary and wiring so #208 can focus purely on UI.

## Output to keep in mind

- `MaterialEditorWindow::Open(GameMaterial*)` / `Render()` are the extension points for Issue #208.
- `ResourcesPanel`'s `on_material_open_` callback is wired in `EditorWindow`'s constructor — Issue #208 can extend `MaterialEditorWindow::Open()` without touching the wiring.
- Procedural scene materials use `"__proc_"` id prefix to be hidden from the Resources panel. Any future programmatically created material that should stay internal must follow this convention.
- `EditorScene::AddGameMaterial()` should be used for any externally loaded `GameMaterial*` whose lifetime should be tied to the scene.

## Skills / CLAUDE.md instructions used

- `src/CLAUDE.md`: one class per `.h`/`.cpp` pair, Google C++ style, include root is `src/`.
- `src/editor/CLAUDE.md`: one class per `.h`/`.cpp` pair; editor is the dependency-graph leaf; all ImGui calls between `NewFrame()` and `Render()`.
- Project `CLAUDE.md`: conventional commits, PR to `dev`, history file required.
