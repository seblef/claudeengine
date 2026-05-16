# Undo / Redo toolbar buttons and keyboard shortcuts (#233)

**Date:** 2026-05-16
**Branch:** feat/undo-redo-toolbar-233
**Issue:** #233

---

## Summary

Exposed undo/redo to the user via toolbar buttons and `Ctrl+Z` / `Ctrl+Shift+Z`
keyboard shortcuts. This builds directly on the `EditorCommandHistory`
infrastructure added in #232.

---

## Changes

### `src/editor/EditorToolbar.h`

- Added `#include "editor/EditorCommandHistory.h"`.
- Added `void SetCommandHistory(EditorCommandHistory* history)` — must be called
  once after construction.
- Added private member `EditorCommandHistory* history_ = nullptr`.
- Updated class-level doc comment to reflect the new layout.

### `src/editor/EditorToolbar.cpp`

- **Keyboard shortcuts** (`Ctrl+Z` / `Ctrl+Shift+Z`) are checked before the
  ImGui window is opened, so they fire every frame regardless of whether the
  toolbar window is visible. Both shortcuts are gated on
  `!io.WantCaptureKeyboard` (same guard used for existing tool shortcuts) so
  they are silent while a text input widget is focused.
- **Undo button** (`ICON_FA_ROTATE_LEFT`) — disabled via
  `ImGui::BeginDisabled(!history_->CanUndo())`. Clicking calls `Undo()`.
  Tooltip: `"Undo (Ctrl+Z)"`.
- **Redo button** (`ICON_FA_ROTATE_RIGHT`) — same pattern. Tooltip:
  `"Redo (Ctrl+Shift+Z)"`.
- A vertical separator + `SameLine()` separates the undo/redo group from the
  existing transform tools, matching the layout spec in the issue.
- The whole undo/redo block is wrapped in `if (history_)`, so the toolbar
  degrades gracefully if `SetCommandHistory()` is never called.

### `src/editor/EditorWindow.cpp`

- Added `toolbar_->SetCommandHistory(&history_)` in the constructor, immediately
  before `viewport_->SetScene(scene_.get())`.

---

## Decisions and rationale

| Decision | Rationale |
|---|---|
| Guard shortcuts with `WantCaptureKeyboard` instead of `WantTextInput` | Issue spec used `WantCaptureKeyboard`; it is the broader guard that also suppresses shortcuts when any ImGui widget (not just text fields) has keyboard focus. |
| Shortcuts checked outside the `ImGui::Begin` block | Ensures `Ctrl+Z` fires even if the toolbar is docked/hidden on the current frame. Consistent with how existing tool shortcuts were already structured (pre-window). |
| `if (history_)` guard around the whole block | Makes the toolbar safe to use standalone in tests without wiring a history object. |

---

## Skills and instructions used

- Followed `src/CLAUDE.md`: one class per `.h/.cpp`, Google C++ style guide,
  include root is `src/`, ran `cpplint` before commit.
- Followed `src/editor/CLAUDE.md`: all ImGui calls inside `NewFrame/Render`,
  one class per file pair.
- Conventional commit message format.
- History file in `history/` folder as required.

---

## Output to keep in mind for next features

- `EditorToolbar` now holds a `history_` pointer. Any future toolbar refactor
  must preserve the `SetCommandHistory` wiring in `EditorWindow`.
- `WantCaptureKeyboard` is now the canonical guard for all editor keyboard
  shortcuts (the existing tool shortcuts used `WantTextInput` — this PR updated
  the Undo/Redo shortcuts to use the stricter guard per the issue spec, but did
  not change the transform-tool shortcuts to avoid scope creep).
- The undo/redo buttons are conditionally rendered only when `history_` is set;
  if a panel is ever added that needs its own history, a separate toolbar
  instance or a different wiring approach will be needed.
