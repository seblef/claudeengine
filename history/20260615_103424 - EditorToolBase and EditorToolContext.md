# EditorToolBase and EditorToolContext — Foundation for the Abstract Tool System

**Issue**: #507  
**Branch**: `feat/editor-tool-base-507`  
**Date**: 2026-06-15

## Summary

Introduces the abstract tool infrastructure that all concrete editor tools will build on (issue #507 in the "Abstract Editor Tool System" milestone). Two headers and a subdirectory CMakeLists are the entire change; no existing runtime behaviour is altered.

## Files created

| File | Purpose |
|---|---|
| `src/editor/tools/EditorToolContext.h` | Plain non-owning aggregate passed to tools |
| `src/editor/tools/EditorToolBase.h` | Abstract base with five virtual hooks |
| `src/editor/tools/CMakeLists.txt` | Placeholder; will collect `.cpp` sources as tools are added |

## Files modified

| File | Change |
|---|---|
| `src/editor/CMakeLists.txt` | `add_subdirectory(tools)` |
| `src/editor/EditorViewport.h` | Added `#include "editor/tools/EditorToolBase.h"`, new `SetActiveTool(EditorToolBase*)` declaration, and `active_tool_base_` member |
| `src/editor/EditorViewport.cpp` | Implemented `SetActiveTool(EditorToolBase*)` |

## Key decisions

**Forward-declarations in `EditorToolContext.h`** — The struct holds only raw pointers, so including the full headers of `EditorScene`, `GameCamera`, etc. is unnecessary. Forward declarations keep compile time low and avoid pulling half the engine into every tool header.

**Separate overload, not replacement** — The existing `SetActiveTool(EditorTool)` is kept intact so no callsites break. The new `SetActiveTool(EditorToolBase*)` overload is non-owning, which matches the general editor philosophy (subsystems are owned by `EditorSystem`, not by each other).

**Empty tools CMakeLists** — No `.cpp` files exist yet. A comment-only CMakeLists avoids a CMake warning from an empty `target_sources` call while still wiring up the subdirectory so future additions are a one-liner.

**`EditorToolContext` built at activation time** — `SetActiveTool(EditorToolBase*)` constructs the context from the fields already available on `EditorViewport` (`scene_`, `camera_`, `picking_acc_`, `history_`, `video_`). This avoids a persistent context member and keeps the data flow explicit.

## Output to keep in mind for next features

- Concrete tools should live in `src/editor/tools/` and add their `.cpp` to `src/editor/tools/CMakeLists.txt` via `target_sources(editor PRIVATE ...)`.
- `EditorViewport::SetActiveTool(EditorToolBase*)` already calls `OnDeactivate` on the outgoing tool and `OnActivate` on the incoming one — callers just swap the pointer.
- The old `SetActiveTool(EditorTool)` overload is still in place; removing it is tracked in `#refactor-viewport`.

## Skills / CLAUDE.md instructions used

- `impl-issue` skill
- `src/editor/CLAUDE.md` — one class per `.h`/`.cpp` pair, no singletons, ImGui only inside frame brackets
- `src/CLAUDE.md` — Google C++ style, include root is `src/`, cpplint before commit
- Root `CLAUDE.md` — conventional commits, history file, feature branch from `dev`
