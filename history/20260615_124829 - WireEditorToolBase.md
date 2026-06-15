# Wire EditorToolBase into EditorViewport (#512)

## Summary

Completed the final step of the abstract editor tool system: `EditorViewport` is now a thin host that holds only `EditorToolBase*` and delegates all tool-specific logic through that pointer. The `EditorTool` enum has been relocated and all dead state removed.

## Changes

### `src/editor/EditorTool.h` — deleted

The file existed solely to expose the `EditorTool` enum and its `IsCreationTool()` / `IsTransformTool()` helpers to `EditorViewport`. Since the viewport no longer needs the enum, the file was deleted and its contents moved to the natural owner.

### `src/editor/EditorToolbar.h` — enum absorbed

`EditorTool`, `IsCreationTool()`, and `IsTransformTool()` are now defined at the top of this file, before the `EditorToolbar` class declaration. This is the right home: the toolbar is the component that tracks which tool button is active; consumers (`EditorWindow`) already include `EditorToolbar.h`.

The `#include "editor/EditorTool.h"` was replaced by the inline definitions.

### `src/editor/EditorViewport.h` — cleaned

Removed:
- `#include "editor/EditorTool.h"` (deleted file)
- `#include "editor/tools/SelectionTool.h"` (moved to .cpp; forward-declared in .h)
- `SetSelectionActive(bool)` — SelectionTool now manages its own enabled/disabled state
- `SetActiveTool(EditorTool)` — old enum overload, no longer meaningful
- `bool selection_active_` field
- `EditorTool active_tool_` field

Changed:
- `SelectionTool selection_tool_` (value member) → `std::unique_ptr<SelectionTool> selection_tool_` (forward-declared type; .cpp provides the definition)
- `~EditorViewport() = default` moved to .cpp (required so the destructor sees the complete `SelectionTool` type through the unique_ptr deleter)

The public interface is now `SetActiveTool(EditorToolBase*)` only. Passing `nullptr` falls back to `SelectionTool`.

### `src/editor/EditorViewport.cpp` — updated

- Added `#include "editor/tools/SelectionTool.h"`
- Defined `EditorViewport::~EditorViewport() = default` (see above)
- Initialised `selection_tool_` via `std::make_unique<SelectionTool>()` in the member-initialiser list
- Updated `&selection_tool_` → `selection_tool_.get()` at the three call sites

### `src/editor/EditorWindow.h` — updated

- Replaced `#include "editor/EditorTool.h"` with `#include "editor/EditorToolbar.h"` (which now contains the enum)
- Removed the `class EditorToolbar;` forward declaration (the full include makes it redundant)

### `src/editor/EditorWindow.cpp` — updated

Removed the two per-frame calls that were now vestigial:
```cpp
viewport_->SetSelectionActive(active_tool == EditorTool::kSelection ||
                              IsTransformTool(active_tool));
viewport_->SetActiveTool(active_tool);  // enum overload
```
The `SetSelectionActive` concept no longer exists in the viewport; `SelectionTool` controls its own behaviour. The enum-based `SetActiveTool` did nothing useful (it only stored a dead `active_tool_` field that the renderer never read).

All other `EditorTool` usages in `EditorWindow.cpp` (`ToolToLightType`, `IsCreationTool`, various `kXxx` comparisons) continue to compile because they pull the enum transitively via `EditorToolbar.h`.

## Decisions

**Why move `EditorTool` to `EditorToolbar.h` instead of deleting it outright?**
`EditorWindow` still needs the enum to dispatch toolbar state to the right `EditorToolBase` subclass. `EditorToolbar` is the authoritative holder of the active tool value, so the enum belongs alongside its class definition.

**Why `unique_ptr<SelectionTool>` instead of a value member?**
With a value member, the class definition of `SelectionTool` must be visible in `EditorViewport.h`. A `unique_ptr` with a forward declaration keeps the include contained to `EditorViewport.cpp`, satisfying the acceptance criterion "EditorViewport.h no longer includes tool-specific headers".

**Why a user-defined destructor in .cpp?**
`std::unique_ptr<T>` requires `T` to be a complete type at the point where the deleter is instantiated. With `~EditorViewport() = default` in the header, the compiler instantiates the deleter inside the header where `SelectionTool` is incomplete. Moving the destructor definition to the .cpp (where `SelectionTool.h` is included) resolves this.

## Output / keep in mind for future features

- `EditorTool` enum now lives in `EditorToolbar.h`. Any new file that needs to compare tool values must include that header.
- `EditorViewport` public API: only `SetActiveTool(EditorToolBase*)` remains. No enum overload, no `SetSelectionActive`.
- The `SelectionTool` instance is owned by `EditorViewport` (via `unique_ptr`) and is the fallback when `nullptr` is passed to `SetActiveTool`.

## Skills and CLAUDE.md instructions followed

- `impl-issue` skill
- CLAUDE.md: conventional commits, history file, one class per `.h`/`.cpp` pair, Google C++ style guide, no unnecessary features beyond the task scope.
- `src/editor/CLAUDE.md`: GUI vs. edition logic separation, one class per file pair.
