# Fix: Floor Plane Does Not Disappear When Loading a Map (#370)

## Problem

When the editor starts, `EditorScene` is constructed which creates a floor plane
(`__proc_editor_floor`) and a cube (`__proc_editor_cube`) as default dynamic objects.
These are procedural objects and are intentionally excluded from serialization
(filtered by the `__proc_` prefix in `MapSerializer::SerializeVisitor::Visit`).

The bug: `MapSerializer::Load` also calls the parameterised `EditorScene` constructor,
which unconditionally creates the floor and cube. The loaded map's objects were then
added on top, leaving both the stale defaults and the real map content in the scene.

## Root Cause

`EditorScene(video, name, size, light)` always created floor + cube regardless of
whether it was building a fresh "new map" scene or reconstructing a scene from a file.

## Fix

Added a `bool add_default_objects = true` parameter to the parameterised constructor.

- **Default (`true`)**: existing callers (editor startup, "New Map" dialog) are unaffected.
- **`false`**: `MapSerializer::Load` passes this value so loaded scenes contain only the
  objects from the file.

### Files changed

| File | Change |
|---|---|
| `src/editor/EditorScene.h` | Added `bool add_default_objects = true` to parameterised constructor |
| `src/editor/EditorScene.cpp` | Guard floor/cube creation with the flag; moved `light_` init to initializer list (cppcheck) |
| `src/editor/MapSerializer.cpp` | Pass `false` when constructing scene during `Load()` |

## Decisions

- Chose a **default-true boolean flag** over a factory method or tag struct to keep the
  change minimal and not break any existing callsites.
- Both floor plane and cube are gated together — they are conceptually the "default new
  map" objects and should either both appear or both be absent.
- The `light_` member was moved to the initializer list to satisfy the existing cppcheck
  pre-commit hook (`useInitializationList`).

## Notes for future contributions

- `__proc_` prefixed mesh templates are always procedural/ephemeral — they are
  intentionally excluded from serialization. Keep this invariant when adding new
  procedural objects.
- `EditorScene(video)` (default ctor) → new map with floor + cube.  
  `EditorScene(video, name, size, light, false)` → bare scene for loading.

## Skills / instructions referenced

- `src/CLAUDE.md`: Google C++ style, one class per file, include paths from `src/`
- `src/editor/CLAUDE.md`: editor is the leaf of the dependency graph
- Root `CLAUDE.md`: conventional commits, PR to `dev`, history file required
