# PropertiesPanel: light and mesh properties edition

**Issue:** #222  
**Date:** 2026-05-16  
**Branch:** feat/properties-panel-222

---

## Summary

Added a `PropertiesPanel` class that fills the previously empty "Properties" docked panel in `EditorWindow`. It shows editable properties for the selected scene object (lights or meshes) with immediate viewport feedback.

---

## New files

### `src/editor/EditorUtils.h`
Header-only utilities for direction editing:
- `Vec3fToYawPitch(const core::Vec3f&)` — converts a normalized direction to `{yaw, pitch}` in degrees using `atan2` / `asin`.
- `YawPitchToVec3f(float yaw, float pitch)` — inverse: degrees → normalized `core::Vec3f`.

Kept header-only (all `inline`) as specified in the issue to avoid a gratuitous translation unit.

### `src/editor/PropertiesPanel.h` / `.cpp`

`PropertiesPanel::Render(game::GameObject*)` dispatches on `GetType()`:
- **No selection** → grey italic label "No object selected" using `ImGuiCol_TextDisabled`.
- **Mesh** → `ImGui::LabelText("Template", ...)` showing `MeshTemplate::GetId()`.
- **Light** → full light property editor.

#### Light property editor design

**Common fields** (all light types): Color (`ColorEdit3`), Intensity (`DragFloat`), Cast Shadow (`Checkbox`), Shadow Resolution (`Combo` over {256, 512, 1024, 2048}), Shadow Bias (`DragFloat`).

**Type-specific sections** are always rendered to keep layout stable, but disabled via `ImGui::BeginDisabled(!is_<type>)` for non-matching types. This means dummy widgets (with zero values) are shown greyed out for inapplicable sections:
- **OmniLight** — Radius
- **CircleSpotLight** — Inner/Outer Angle (°), Range, Yaw/Pitch direction
- **RectangleSpotLight** — H/V Angle (°), Range, Yaw/Pitch direction
- **GlobalLight** — Yaw/Pitch direction, Ambient color

**ImGui ID disambiguation** — widgets that appear in multiple type-specific blocks (e.g. "Range", "Yaw (°)") use `##<suffix>` in the ID for the disabled dummy variants to avoid `PushID` collisions between blocks rendered in the same frame.

#### Yaw/Pitch direction editing
Used in place of a raw `Vec3f` editor for usability. Angles are stored internally in radians by the renderer; the panel converts to/from degrees before display.

---

## Modified files

| File | Change |
|---|---|
| `src/editor/CMakeLists.txt` | Added `PropertiesPanel.cpp` to editor library sources |
| `src/editor/EditorWindow.h` | Forward-declared `PropertiesPanel`; added `std::unique_ptr<PropertiesPanel> properties_panel_` member |
| `src/editor/EditorWindow.cpp` | Included `PropertiesPanel.h`; constructed `properties_panel_` in initializer list; wired `properties_panel_->Render(scene_->GetSelectedObject())` inside `ImGui::Begin("Properties")` |

---

## Decisions and rationale

1. **Layout stability via BeginDisabled** — always rendering all type-specific sections (disabled when not applicable) ensures the panel height doesn't change as the user selects different light types, preventing visual layout jumps.

2. **Dummy zero values in disabled blocks** — disabled widgets show `0` as placeholder. This is visually acceptable since they are greyed out and clearly inactive.

3. **Templated `RenderDirectionWidgets`** — avoids code duplication across `CircleSpotLight`, `RectangleSpotLight`, and `GlobalLight` (all share the same `GetDirection()` / `SetDirection()` interface).

4. **Immediate application** — no "Apply" button; all changes are forwarded to the renderer on every frame where a widget is modified, consistent with ImGui's immediate-mode paradigm.

5. **`SeparatorText`** — used for light type and mesh section headings, available in ImGui 1.89+.

---

## Output for next features

- `PropertiesPanel` is the natural place to add **transform editing** (position/rotation/scale via the world matrix) in a future milestone — see `GameObject::SetWorldTransform()`.
- If "undo" support is added later, each setter call in `PropertiesPanel` is an obvious capture point for command objects.
- The `EditorUtils.h` `Vec3fToYawPitch` / `YawPitchToVec3f` helpers can be reused by any future editor panel that needs direction editing.

---

## Skills and instructions followed

- `impl-issue` skill  
- `src/CLAUDE.md`: one class per `.h`/`.cpp` pair, Google C++ style, include root is `src/`
- `src/editor/CLAUDE.md`: all ImGui calls bracketed by `NewFrame`/`Render`, panels owned by `EditorSystem`, no singletons
- Git workflow: feat branch → implement → cpplint → commit → PR to dev
