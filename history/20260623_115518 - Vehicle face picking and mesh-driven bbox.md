# Vehicle face picking and mesh-driven bbox

**Issue:** #762  
**Branch:** `feat/vehicle-face-picking-mesh-bbox`

---

## Changes

### `src/game/GameVehicle.h`
- Forward-declared `MeshTemplate` in the `game` namespace.
- Added public getter `GetBodyTemplate() const` returning `MeshTemplate*`
  (non-owning; used by `PickingUtils` for ray-triangle intersection).

### `src/game/GameVehicle.cpp`
- **Bbox source changed:** the local bbox passed to `GameObject` is now taken
  from `tmpl->GetBodyTemplate()->GetLocalBBox()` (computed from geometry at
  load time) instead of the lambda that built a symmetric box from
  `VehicleDesc::half_extents` (a Jolt physics parameter).  
  `half_extents` is left untouched — it continues to drive the Jolt box
  collider in `PhysicsSystem::CreateVehicle()`.
- Removed the now-unused `#include "core/BBox3.h"`.
- Implemented `GetBodyTemplate()` delegating to `template_->GetBodyTemplate()`.

### `src/editor/tools/PickingUtils.cpp`
- Added `#include "game/GameVehicle.h"` and `#include "game/VehicleTemplate.h"`.
- **Vehicle pass rewritten** from a pure bbox hit to a two-level strategy
  matching the mesh pass:
  1. World bbox early-out (fast reject, avoids per-triangle work).
  2. Ray-triangle intersection in model space against the body mesh's CPU
     positions/indices.
  3. Fall back to accepting the bbox hit when `GetCPUPositions()` is empty
     (procedural/preview templates).

---

## Decisions and rationale

**Why use the body mesh bbox instead of `half_extents`?**  
`half_extents` sizes the Jolt rigid body, not the visual mesh.  Any mismatch
between the two (overhangs, spoilers, body shapes that differ from a box) makes
the selection wireframe misleading.  `MeshTemplate::GetLocalBBox()` is already
derived from the geometry vertices at load time, so it is always exact and
requires no manual maintenance.

**Why keep `half_extents` unchanged?**  
It is a physics parameter consumed by Jolt.  Changing it to match the visual
mesh would alter collision behaviour and is out of scope for an editor fix.

**Why the two-level strategy (bbox → triangles)?**  
Pure triangle iteration over all candidates is O(n × t) where t is the
triangle count.  The world-bbox early-out (already maintained by `GameObject`)
prunes the vast majority of vehicles without touching the CPU geometry.

**No change to `PickingAccelerator`** — `kVehicle` was already registered in
`IsPickable()` and the accelerator works on world bboxes, which are still valid
after this change.

---

## Files affected

| File | Change |
|---|---|
| `src/game/GameVehicle.h` | Forward-declare `MeshTemplate`; add `GetBodyTemplate()` |
| `src/game/GameVehicle.cpp` | Bbox from body mesh bbox; implement getter; drop unused include |
| `src/editor/tools/PickingUtils.cpp` | Vehicle pass: bbox early-out + ray-triangle |

---

## Skills and instructions used

- `impl-issue` skill for the implementation workflow.
- `CLAUDE.md` (root): conventional commits, history file, branch naming.
- `src/CLAUDE.md`: Google C++ style, one class per file, include root is `src/`.
- `src/game/CLAUDE.md`: `GameObject` invariants, `MeshTemplate` ref-count rules.
- `src/editor/CLAUDE.md`: editor is the dependency graph leaf; no game imports
  of editor code introduced.

---

## Notes for future work

- If wheel meshes are ever wanted for sub-part picking (e.g. select an
  individual wheel), the same pattern can be applied to `GetFrontWheelTemplate()`
  / `GetRearWheelTemplate()` with each wheel's world transform.
- CPU geometry retention (`GetCPUPositions` / `GetCPUIndices`) is already in
  place for all file-backed `MeshTemplate` instances.  Procedural templates
  used in the vehicle preview panel are the only case where the fallback path
  is exercised.
