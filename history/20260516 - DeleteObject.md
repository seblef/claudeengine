# Object Deletion on Delete Key (#223)

## Summary

Added Delete-key handling in `EditorViewport::OnEvent` so that pressing Delete
removes the selected dynamic object from the scene and clears the selection.

## Changes

### `src/editor/EditorViewport.cpp`

- Added `#include <loguru.hpp>` for the deletion log message.
- Extended `OnEvent` with a `kKeyDown` / `kDelete` branch that:
  1. Returns early (no-op) if ImGuizmo is actively dragging (`ImGuizmo::IsUsing()`).
  2. Retrieves the selected object from the scene.
  3. Checks `scene_->IsDynamic(selected)` — floor and sun light cannot be deleted.
  4. Calls `scene_->RemoveDynamicObject(selected)`, which already clears
     `scene_->selected_` internally.
  5. Clears `EditorViewport::selected_object_` to prevent a dangling pointer.
  6. Logs the deletion at INFO level.

## Decisions and rationale

- **No confirmation dialog** — not required for this milestone.
- **ImGuizmo guard** — if the user is mid-drag the Delete key is almost certainly
  accidental; skipping deletion prevents surprising loss of just-moved objects.
- **Log message** — keeps the event visible in the Logs panel for easier debugging.
- **`selected_object_` cleared** — the field is exposed via `GetSelectedObject()`;
  clearing it avoids a dangling pointer even though the viewport's own rendering
  reads `scene_->GetSelectedObject()` directly.

## Key facts for next features

- `core::Key::kDelete` already existed in `src/core/Key.h` — no changes needed.
- `EditorScene::RemoveDynamicObject` is the single point that unregisters the
  object from `GameSystem`, removes it from `objects_` / `dynamic_objects_`, and
  clears `scene_->selected_`. Callers only need to clear their own raw pointers.
- `IsDynamic` guards static objects (floor, global light) automatically; no
  additional type-checking is required at the call site.

## Skills and CLAUDE.md rules applied

- Followed the git workflow: `dev` → feature branch → implement → cpplint → PR.
- One-line log comment only where the `ImGuizmo::IsUsing()` guard needs context.
- No undo/redo wired in (out of scope until milestone 9).
