# GamePivot — Pure Transform Node (Issue #656)

## Summary

Introduced `GamePivot`, a concrete `GameObject` subclass with no geometry, physics, or
light.  It acts as a logical group anchor in the scene graph: attach children to a pivot
and move/rotate them as a unit via a single transform with no visual representation.

## Changes

### `src/game/GameObjectType.h`
Added `kPivot` enumerator.

### `src/game/GameObjectVisitor.h`
Forward-declared `GamePivot` and added `virtual void Visit(GamePivot& pivot) = 0`.

### `src/game/GamePivot.h` / `GamePivot.cpp` (new files)
- Constructor passes `GameObjectType::kPivot` and a degenerate `BBox3{}` to `GameObject`.
- `OnWorldTransformUpdated()`, `OnAddedToScene()`, `OnRemovedFromScene()` are all no-ops.
- `Copy(position)` clones the pivot at `position`, preserving orientation/scale from the
  world transform (same pattern as `GameMesh::Copy`).  Children are not copied here —
  the deep-copy logic in `GameObject` handles that.
- `Accept()` calls `visitor.Visit(*this)`.

### `src/game/CMakeLists.txt`
Added `GamePivot.cpp` to the `game` static library.

### `src/game/MapLoader.cpp`
Added `#include "game/GamePivot.h"`, `ParsePivot()` helper, and dispatch for `type == "pivot"`.
The YAML token is `"pivot"` (no extra fields beyond `name` and `transform`).

### `src/editor/MapSerializer.h` / `MapSerializer.cpp`
Added `Visit(game::GamePivot&)` to `SerializeVisitor`.  Emits `type: pivot` plus `name` and
`transform`; no extra fields.

### `src/editor/EditorToolbar.h` / `EditorToolbar.cpp`
Added `EditorTool::kCreatePivot` to the enum and `IsCreationTool()`.
Added a toolbar button (crosshairs icon, `ICON_FA_CROSSHAIRS`, tooltip "Create Pivot").

### `src/editor/EditorWindow.cpp`
Added `#include "game/GamePivot.h"` and a `kCreatePivot` branch in the tool-activation
block: instantiates `GamePivot`, names it via `GenerateObjectName`, and enters placement
mode exactly like `kCreatePlayerStart`.

### `src/editor/ObjectsPanel.cpp`
Added `kPivot → ICON_FA_CROSSHAIRS` in `IconForType()` and a "Pivots" type-category group
in `Render()`.

## Decisions

- **Icon choice (`ICON_FA_CROSSHAIRS`)**: conveys "anchor/origin" without implying geometry.
  The same icon is reused on the creation toolbar button for visual consistency.
- **No extra YAML fields**: a pivot carries only transform data, so the serialisation is
  identical to `player_start` in structure — only the `type` token differs.
- **`Copy` preserves orientation**: follows the pattern in `GameMesh::Copy` — replaces only
  the translation column of the world transform.

## What to keep in mind

- Every new concrete `GameObject` subclass requires a `Visit()` overload in
  `GameObjectVisitor` **and** in every class that implements it (currently only
  `MapSerializer::SerializeVisitor`).
- The `kPivot` enumerator is intentionally the last entry in `GameObjectType`; if the enum
  is serialised to disk anywhere else in the future, insertion order must be preserved.

## Skills / instructions used

- `impl-issue` skill
- `src/CLAUDE.md` — one class per `.h`/`.cpp`, include root `src/`, follow Google C++ style
- `src/game/CLAUDE.md` — pattern for `OnAddedToScene` / `OnRemovedFromScene` no-ops
- `src/editor/CLAUDE.md` — dependency graph invariant, editor is the leaf
