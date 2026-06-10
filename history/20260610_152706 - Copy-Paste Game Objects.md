# Copy/Paste Game Objects — Issue #446

## What changed

Added support for copying and pasting game objects in the editor. Users can duplicate meshes and lights using Ctrl+C / Ctrl+V or the new toolbar buttons.

## Files modified

### `src/game/`
- **`GameObject.h` / `.cpp`** — Added a virtual `Copy(const core::Vec3f& position)` method that returns `nullptr` by default. Non-copyable types (GameCamera, GameTerrain, GamePlayerStart) inherit the default and require no changes.
- **`GameMesh.h` / `.cpp`** — Override `Copy()`: constructs a new `GameMesh` from the same `MeshTemplate`, preserves the original orientation/scale, sets position to the provided value, appends " (copy)" to the name.
- **`GameLight.h` / `.cpp`** — Override `Copy()`: extracts all renderer light properties inline (cannot use the editor `CaptureSnapshot` helper since `game` must not depend on `editor`), constructs a `GameLightDesc`, creates a new `GameLight` with identical type and properties at the given position.

### `src/editor/`
- **`EditorToolbar.h` / `.cpp`** — Added `SetOnCopy`, `SetOnPaste`, `SetCanCopy`, `SetCanPaste`. Added Copy (`ICON_FA_COPY`) and Paste (`ICON_FA_PASTE`) buttons between Save and the transform-tool separator. Added Ctrl+C / Ctrl+V keyboard shortcuts. Fixed a pre-existing shortcut conflict: single-key tool shortcuts (Q/W/E/R/**C**) now check `!io.KeyCtrl && !io.KeyAlt` so Ctrl+C no longer activates the Camera tool.
- **`EditorWindow.h` / `.cpp`** — Added `clipboard_` (`unique_ptr<game::GameObject>`) as a master clone that is never added to the scene. `CopySelectedObject()` calls `obj->Copy(original_pos)` to produce the master. `PasteObject()` calls `clipboard_->Copy(pos + (1, 0, 1))` and wraps the result in a `PlaceObjectCommand` (undo/redo supported). The can_copy/can_paste state is updated every frame based on the current selection and clipboard state.

## Design decisions

### `GameObject::Copy(position)` virtual method
**Rationale**: The editor should be agnostic to concrete object types. By putting cloning responsibility in the game layer, the editor's copy/paste logic works for any future new object type that opts in by overriding `Copy()`.

### Non-pure virtual with `nullptr` default
Rather than making `Copy()` pure virtual (which would require all subclasses to implement it), the default returns `nullptr`. This means terrain and player start are automatically non-copyable and no changes are needed in those classes. The editor checks `GetType()` directly for the toolbar button state (no allocation per frame).

### Clipboard stores a master clone
On copy, we immediately call `Copy(original_pos)` and own the result in `clipboard_`. On each paste, we call `clipboard_->Copy(paste_pos)` for a fresh clone. This avoids dangling pointer issues if the original object is later deleted or its placement is undone.

### Paste offset: (+1, 0, +1) from the clipboard master's position
A small diagonal offset makes the copy visible without stacking it exactly on the original. Multiple pastes land at the same offset, which is consistent with common editor behavior.

## Key invariants preserved

- The `game` module does not depend on `editor` — `GameLight::Copy` duplicates the snapshot logic inline rather than reusing `CaptureSnapshot`.
- Paste is fully undoable via the existing `PlaceObjectCommand`.
- No new CMakeLists.txt entries needed.

## Skills and instructions used

- `src/editor/CLAUDE.md` — editor module dependency invariant (editor is leaf, game must not depend on editor)
- `src/CLAUDE.md` — one class per .h/.cpp, Google C++ style
- `src/game/CLAUDE.md` — module structure and key invariants
