# ResourcesPanel — New Material & Import Material buttons (issue #263)

## Changes

### `src/editor/ResourcesPanel.h`
- Added `SetOnNewMaterial(std::function<void(std::string_view)>)` callback setter.
- Added `SetOnImportMaterial(std::function<void()>)` callback setter.
- Added private state: `show_new_mat_modal_` flag and `new_mat_name_buf_[128]` for the
  "New Material" modal.

### `src/editor/ResourcesPanel.cpp`
- Replaced the plain `TreeNodeEx` Materials header with a formatted node
  (`"##mat_header"`, `ICON_FA_PALETTE Materials`) plus two inline `SmallButton`s:
  - `ICON_FA_PLUS` — seeds `new_mat_name_buf_` and raises `show_new_mat_modal_`.
  - `ICON_FA_FILE_IMPORT` — directly fires `on_import_material_`.
- Added a `BeginPopupModal("New Material##modal")` rendered at the end of `Render()`:
  `InputText` for the name, `Create` button (guarded by non-empty name), and `Cancel`.
- Added `#include <cstring>` for `std::strncpy` / `std::strlen`.

### `src/editor/EditorWindow.cpp`
- Added `#include "renderer/MaterialDesc.h"`.
- Wired `SetOnImportMaterial` → `ImportMaterial()` (existing NFD dialog).
- Wired `SetOnNewMaterial` → allocates a `GameMaterial` with a default
  `renderer::MaterialDesc`, registers it via `scene_->AddGameMaterial`, then
  opens it in the `MaterialEditorWindow`.

## Decisions

- **Modal owned by `ResourcesPanel`**: The popup lives where its trigger lives,
  keeping all modal state local; `EditorWindow` only receives the confirmed name.
- **`GetWindowWidth() - 50.f` alignment**: matches the spec; both small buttons fit
  in ~50 px at default font scale.
- **Default `MaterialDesc()`**: zero-initialised descriptor produces a blank but
  valid material — the user customises it in the material editor immediately after
  creation.

## Output to keep in mind

- `GameMaterial` allocates into the global registry on construction; calling
  `scene_->AddGameMaterial` registers it with the scene as well.
- `ImportMaterial()` in `EditorWindow` already handles NFD lifecycle; reusing it
  from the callback keeps the import path DRY.

## Skills / instructions used

- `impl-issue` skill
- `src/editor/CLAUDE.md` — one class per file, ImGui lifecycle rules
- `src/CLAUDE.md` — Google C++ style, include paths relative to `src/`
- CLAUDE.md git workflow (branch from dev, cpplint, conventional commit, PR to dev)
