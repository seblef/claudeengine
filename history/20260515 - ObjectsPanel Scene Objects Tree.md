# ObjectsPanel — Scene Objects Tree (Issue #176)

## Changes

### `src/game/GameObject.h` / `src/game/GameObject.cpp`
- Added `name_` field (`std::string`, default `"Object"`) with `SetName()` / `GetName()` accessors.
- Added `#include <string>` to the header.
- `name_` carries a `// cppcheck-suppress unusedStructMember` annotation to match the existing convention on other private fields.

### `src/editor/EditorScene.cpp`
- Set `SetName("Floor")` on `floor_`, `SetName("Cube")` on `cube_`, `SetName("Sun")` on `light_` immediately after construction.

### `src/editor/ObjectsPanel.h`
- Updated signature from `void Render()` to `void Render(EditorScene& scene)` (non-const — calls `SetSelectedObject`).
- Added forward declaration `class EditorScene`.

### `src/editor/ObjectsPanel.cpp`
- Anonymous-namespace helper `RenderGroup(btn_id, group_name, icon_color, type, objects, scene)`:
  - `ColorButton` icon + `SameLine` + `TreeNodeEx(group_name, DefaultOpen)`.
  - Iterates `objects`, skips mismatched types.
  - Per object: `PushID(obj)` / `PopID()` for stable ImGui IDs; `PushStyleColor(ImGuiCol_Header, kSelectedColor)` + `ImGuiTreeNodeFlags_Selected` when selected; `IsItemClicked()` → `SetSelectedObject(obj)`.
- `Render()` calls `RenderGroup` three times: Meshes (orange `{1,165/255,0,1}`), Lights (yellow `{1,220/255,50/255,1}`), Cameras (light-blue `{100/255,180/255,1,1}`).
- Icon values `255.f/255.f` avoided — cppcheck flags `duplicateExpression` for same-expression division; replaced with `1.f` literals.

### `src/editor/EditorWindow.cpp`
- Changed `objects_panel_->Render()` → `objects_panel_->Render(*scene_)`.

## Decisions

- **Non-const `Render(EditorScene&)`**: The panel writes selection state, so the scene reference cannot be const (same pattern as any panel that mutates scene state).
- **`PushID(obj)` pointer stability**: Raw pointers are stable for the lifetime of the scene, making them safe as ImGui IDs.
- **Icon colour `255.f/255.f` → `1.f`**: cppcheck `duplicateExpression` fires on `X/X` pattern. All channels that were `255/255` were replaced with `1.f` directly; zero channels use `0.f` directly.
- **Lights selectable**: Lights are added to the selection model (clicking "Sun" sets `selected_`). The viewport ignores lights in ray-casting and gizmo rendering, but selection itself is valid — the item highlights in the tree and future properties panels can read the selection.

## Output to keep in mind

- `GameObject::GetName()` is now available on all `GameObjectType` variants — future panels (e.g. Properties) can display it.
- The Cameras group will always be empty until a `GameCamera` is added to the scene; the group still renders correctly (empty tree body under the header).
- Selection is bidirectional: clicking in the Objects tab selects the object; clicking in the viewport selects it; both update the same `EditorScene::selected_` pointer.

## Skills used
- `projectSettings:impl-issue`

## CLAUDE.md notes taken into account
- One class per `.h`/`.cpp` pair.
- `editor` is the dependency-graph leaf — `game` was extended without importing any `editor` headers.
- Google C++ style guide; cpplint + cppcheck pre-commit hooks pass.
- Include root is `src/`.
