# Editor cleanup: delete legacy gizmo renderers, add PlayerStartGizmos, update EditorViewport

**Issue:** #529  
**Branch:** `feat/editor-cleanup-delete-legacy-gizmo-renderers-529`  
**Milestone:** WireframeRenderer — unified debug draw

---

## Summary

This is the final integration step of the WireframeRenderer milestone. Three
legacy editor gizmo classes are removed, a lightweight free function replaces
`PlayerStartGizmoRenderer`, and `EditorViewport` is updated to drive the unified
`WireframeRenderer` API.

---

## Changes

### New: `editor/PlayerStartGizmos.h` + `editor/PlayerStartGizmos.cpp`

Free function `editor::EnqueuePlayerStartGizmos()` iterates the scene object list,
filters for `kPlayerStart` type, extracts world position from column 3 of the
world transform, then calls `WireframeRenderer::PushOverlaySegment()` for:

- A white pole (height 2.0) from `base` to `top`.
- Three flag triangle edges (width 0.8, height 0.5) at the pole top.

Flag colour is brighter green `(0.2, 1.0, 0.467, 1.0)` when `IsHighlighted(obj)`
returns true (i.e., `selected_key == obj`), normal green `(0.0, 0.8, 0.267, 1.0)`
otherwise. Geometry constants match the deleted `PlayerStartGizmoRenderer` exactly.

### Deleted

- `editor/LightWireframeRenderer.h` and `.cpp` — superseded by `WireframeRenderer`
  (lights push their own wireframes via `Light::Enqueue()`, issues #526).
- `editor/ParticleGizmoRenderer.h` and `.cpp` — superseded by `WireframeRenderer`
  (particles push via `ParticleRenderable::Enqueue()`, issue #527).
- `editor/PlayerStartGizmoRenderer.h` and `.cpp` — replaced by the new free function.

### Updated: `editor/EditorViewport.h`

- Removed `#include` of the three deleted headers.
- Removed member fields `light_wireframe_`, `player_start_gizmo_`, `particle_gizmo_`.
- Removed `terrain_wireframe_debug_` bool member.
- Changed `SetTerrainWireframeDebugEnabled()` from inline to a `.cpp`-defined method
  (forward to `Renderer::Instance().SetTerrainWireframeEnabled(enabled)`).

### Updated: `editor/EditorViewport.cpp`

- Removed constructor initializers for the three deleted members.
- Removed the `SetTerrainWireframeEnabled(terrain_wireframe_debug_)` call from
  `Render()` (now called by `SetTerrainWireframeDebugEnabled` on change instead).
- Replaced the four legacy render blocks with:
  1. `WireframeRenderer::SetHighlightedObject(scene_->GetSelectedObject())`.
  2. `EnqueuePlayerStartGizmos(scene_->GetObjects(), scene_->GetSelectedObject())`.
  3. `WireframeRenderer::Render(*camera, wireframe_fbo_, render_fbo_)`.
- Added `#include "editor/PlayerStartGizmos.h"` (WireframeRenderer.h was already included).

### Updated: `editor/CMakeLists.txt`

- Removed: `LightWireframeRenderer.cpp`, `PlayerStartGizmoRenderer.cpp`,
  `ParticleGizmoRenderer.cpp`.
- Added: `PlayerStartGizmos.cpp`.

---

## Decisions

**Free function over class:** The issue spec mandates a free function. Because
`PlayerStartGizmos` has no per-instance state (all geometry is pushed directly to
the singleton `WireframeRenderer`), a class would have added no value.

**`SetTerrainWireframeDebugEnabled` moved to .cpp:** Calling
`Renderer::Instance()` inline in the header would pull `Renderer.h` into every TU
that includes `EditorViewport.h`. Moving the definition to the `.cpp` keeps the
header lean.

**`IsHighlighted` call convention:** The issue specifies
`IsHighlighted(selected_key == obj ? obj : nullptr)`. When `selected_key == obj`
the pointer itself is passed (truthy); otherwise `nullptr` is passed (falsy). This
matches `IsHighlighted`'s contract: `key != nullptr && key == highlighted_key_`.

---

## Dependencies

Built on top of:
- #524 — `WireframeRenderer` creation
- #525 — Lifecycle integration
- #526 — `Light::Enqueue()` wired
- #527 — `ParticleRenderable::Enqueue()` wired

---

## Skills / CLAUDE.md sections used

- `src/CLAUDE.md`: One class per `.h`/`.cpp`, free functions in named `*Utils.cpp`
  (here the module name `PlayerStartGizmos` is more descriptive than `Utils`),
  Google C++ style guide.
- `src/editor/CLAUDE.md`: Editor is the leaf node — no reverse deps into renderer.
  GUI vs. edition logic separation.
- Root `CLAUDE.md`: Git workflow, history file requirement, cpplint step.
