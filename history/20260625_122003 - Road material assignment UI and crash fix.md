# Road material assignment UI + crash fix on null material

**Issue:** #791
**Branch:** feat/road-material-ui

---

## Changes

### Bug fix: null-material crash (`MeshInstance::IsShadowCaster`)

`MeshInstance::IsShadowCaster()` unconditionally dereferenced the material pointer
via `model_->GetMaterial()->GetCastShadow()`. When a `GameRoad` with no material
assigned was added to the scene, this caused an immediate null-pointer crash because
the renderer calls `IsShadowCaster()` on every renderable each frame.

**Fix:** added a null guard:
```cpp
const Material* mat = model_->GetMaterial();
return mat && mat->GetCastShadow();
```
This also protects any future renderable that may temporarily lack a material.

---

### New: `MaterialSelectionModal`

Follows the same pattern as `SoundEmitterSelectionModal`. Scans `data/materials/`
for `.yaml` files and presents a scrollable modal list. `Render()` returns the
chosen material name (basename without `.yaml`) on confirmation, or an empty string
otherwise.

---

### New: `RoadMaterialAssignCommand`

Undoable command that calls `GameRoad::SetMaterial(after_)` on Execute/Redo and
`GameRoad::SetMaterial(before_)` on Undo. Both pointers are null-safe (a road may
have no material before the first assignment). AddRef/Release are called only for
non-null pointers to keep the materials alive for the lifetime of undo history.

**Ref-counting note:** `GameRoad::SetMaterial` does not AddRef the material it is
assigned (unlike MeshTemplate which owns its material). This means the command is
responsible for holding the only extra ref. In `PropertiesPanel::RenderRoadProperties`,
after pushing the command (which AddRef'd `after`), the GetOrLoad ref is Released.
In the no-history edge case, the GetOrLoad ref is intentionally kept alive to prevent
the material from being destroyed while the road uses it.

---

### Updated: `PropertiesPanel`

- Added `SetVideoDevice(abstract::VideoDevice*)` setter (needed to call
  `GameMaterial::GetOrLoad` on material selection).
- Added `MaterialSelectionModal material_picker_modal_` member.
- Added `RenderRoadProperties(game::GameRoad*)` method that shows:
  - Current material name label (`"(none)"` if null)
  - "Change…" button opening the material picker modal
  - On pick: pushes `RoadMaterialAssignCommand` to history
- Dispatched `kRoad` in `Render()`'s switch.

---

### Updated: `MaterialEditorWindow::ApplyToSelection`

Extended to handle `kRoad` in addition to `kMesh`. When a road is selected, the
"Apply" button in the material editor pushes a `RoadMaterialAssignCommand`.

---

## Wiring

`EditorWindow` now calls `properties_panel_->SetVideoDevice(video_)` immediately
after `SetCommandHistory`, so the panel can load materials by name.

---

## Decisions

- **`const_cast` for `GetMaterialPtr()`:** `GameRoad::GetMaterialPtr()` returns
  `const GameMaterial*` to prevent callers from modifying material properties through
  the road's accessor. The editor command needs a mutable pointer for `AddRef`/`Release`
  (which are non-const). `const_cast` is acceptable here because the material IS
  mutable — the const is a logical constraint, not a physical one.
- **No default material on road creation:** kept out of scope per the issue spec.
- **`MaterialSelectionModal` returns a name string** (like `SoundEmitterSelectionModal`)
  rather than a `GameMaterial*`, keeping the modal free of knowledge about video
  devices and resource loading.

---

## Files touched

| File | Change |
|---|---|
| `src/renderer/MeshInstance.h` | Null guard in `IsShadowCaster()` |
| `src/editor/MaterialSelectionModal.h/.cpp` | New modal (new files) |
| `src/editor/commands/RoadMaterialAssignCommand.h/.cpp` | New undoable command (new files) |
| `src/editor/PropertiesPanel.h/.cpp` | `SetVideoDevice`, `RenderRoadProperties`, `kRoad` dispatch |
| `src/editor/MaterialEditorWindow.cpp` | `ApplyToSelection` extended for `kRoad` |
| `src/editor/CMakeLists.txt` | Added `MaterialSelectionModal.cpp` |
| `src/editor/commands/CMakeLists.txt` | Added `RoadMaterialAssignCommand.cpp` |
| `src/editor/EditorWindow.cpp` | `SetVideoDevice` wiring |

## Skills / instructions used

- `impl-issue` skill
- `CLAUDE.md` (project): branch naming, cpplint, conventional commits, history file
- `src/CLAUDE.md`: one class per .h/.cpp, Google C++ style, no more than one class per file
- `src/editor/CLAUDE.md`: panel classes must be pure UI, value members need full include
