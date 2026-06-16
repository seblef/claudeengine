# Transformed Local BBox Rendering (Issue #593)

## What changed

**File**: `src/editor/tools/PickingUtils.cpp`

### `DrawBBoxWireframe` (anonymous namespace helper)

Added a `const core::Mat4f& model` parameter between `bbox` and `vp`. Each of the 8 local-space corners is now transformed to world space via `corner * model` before being projected through VP. The old signature accepted a world-space AABB (already axis-aligned); the new signature accepts a local AABB plus its world transform, producing a true OBB wireframe.

### `DrawSelectedBBox` (public function)

Changed `obj->GetWorldBBox()` → `obj->GetLocalBBox(), obj->GetWorldTransform()` to pass the local AABB and world transform to the updated helper.

## Decisions and rationale

**Why OBB instead of world AABB?** The world AABB is computed by transforming all 8 local corners and taking the min/max, so it grows larger as objects rotate. The OBB (local AABB transformed by the world matrix) stays tight against the geometry at any orientation, which is a much more useful visual cue in the editor.

**Two-step transform (local → world → clip)** rather than pre-computing a combined `mvp` matrix: avoids a 4×4 matrix multiply per draw call (done once per object) while keeping the code readable. Either approach is correct; the two-step version is slightly more explicit.

**No API surface change**: `DrawBBoxWireframe` is in an anonymous namespace, so only `DrawSelectedBBox` calls it. The public signature of `DrawSelectedBBox` (declared in `PickingUtils.h`) is unchanged.

**Grouped objects**: Groups are a pure editor selection concept — they do not affect `GetLocalBBox()` or `GetWorldTransform()` on individual `GameObject`s, so no special handling was needed.

## Matrix convention reminder

The engine uses **column-vector convention**: `v * M` in C++ computes `M * v` mathematically. To transform a local point through world transform then VP, write `local_pt * world_transform` then `world_pt * vp`. The C++ `Mat4f::operator*` is the standard matrix product (row-major storage), so `vp * world_transform` would give the combined matrix if needed.

## Skills and CLAUDE.md sections used

- No skills invoked — straightforward local edit.
- Followed `src/CLAUDE.md`: Google C++ style, no comments explaining what the code does (only the why was documented in this history file), one class per file.
- Followed `editor/CLAUDE.md`: editing logic stays in tool classes (`PickingUtils`), not panel classes.

## Output to keep in mind

- `DrawBBoxWireframe` now requires both `bbox` (local AABB) and `model` (world transform). Any future caller must supply both.
- The rendering of the selection box now correctly handles rotation and non-uniform scale without any additional work.
