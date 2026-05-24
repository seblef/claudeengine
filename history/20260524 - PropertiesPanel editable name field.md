# PropertiesPanel — editable Name field (#267)

## Overview

Added an editable **Name** `InputText` field at the top of the Properties panel,
allowing the selected object to be renamed directly from the properties view.

## Changes

### `src/editor/PropertiesPanel.h`
- Added `#include <string>`.
- Added private member `std::string before_name_` to capture the pre-edit name.

### `src/editor/PropertiesPanel.cpp`
- Added `#include <cstring>`, `#include <string>`, and
  `#include "editor/commands/RenameObjectCommand.h"`.
- Inserted an `ImGui::InputText("Name##propname", …)` block at the top of
  `Render()`, before the `switch (obj->GetType())`:
  - On `IsItemActivated` → snapshot current name into `before_name_`.
  - On `IsItemDeactivatedAfterEdit` → push a `RenameObjectCommand` if the name
    changed and is non-empty.
  - A `ImGui::Separator()` visually separates the name field from type-specific
    properties.

## Design decisions

- **No new wiring**: `history_` was already injected by `EditorWindow`; no
  constructor or CMake changes needed.
- **`before_name_` as plain `std::string`**: consistent with the existing
  `before_snapshot_` pattern for LightPropertyCommand tracking.
- **Guard against empty name**: discarding a rename to an empty string avoids
  creating invalid object names.

## Skills / instructions followed

- `src/CLAUDE.md`: Google C++ style, one class per file, project-relative includes.
- `src/editor/CLAUDE.md`: one class per `.h`/`.cpp` pair, all ImGui calls bracketed
  properly.
- `CLAUDE.md` (root): `cpplint` run before commit, conventional-commit message,
  branch from `dev`, PR to `dev`.

## Output / next steps

- The rename from PropertiesPanel is now fully undoable via `RenameObjectCommand`
  (introduced in #265).
- ObjectsPanel inline rename (#266) and PropertiesPanel rename (#267) are now
  the two entry points for renaming; both share the same command.
