# CSM caster Z-fit: clip casters to the light-space window (peter-panning fix)

**Branch:** `fix/csm-caster-aware-near-far` (follow-up to `2b03db5`)

## Problem

After the caster-aware near/far fit (previous commit `2b03db5`), the shadow
map debug view looked correct (casters render into the depth texture with no
clipping), but shadows were visibly offset/detached from their casters once
projected onto the scene, especially for objects near the ground (y=0) —
classic peter-panning: the shadow appears shifted away from the object's true
contact point, so the "touching the ground" part of the shadow is missing.

Root cause: `ShadowRenderer::RenderCascades`'s caster Z-fit took each
candidate caster's **full, unclipped** world bounding box, transformed it
into light space via `BBox3::operator*(Mat4f)`, and unioned its raw
`GetMin().z`/`GetMax().z` into the cascade's Z range. For a caster whose
world footprint is large relative to the cascade's light-space XY window and
not aligned with the light's view axis (e.g. a long/wide object under a
tilted sun — a road segment, in the reported scenario), the box's own extent
along the light's XY axes folds into an inflated apparent Z spread, even
though only a small sliver of the box actually overlaps the window. This
directly inflates `ls_near`/`ls_far` for the whole cascade.

`shadow_bias` (`data/shaders/glsl/lighting/global_light_ps.glsl`, subtracted
from normalized shadow-map depth) is a flat per-light constant with no
dependency on the cascade's near/far range. Since `OrthoOffCenterRH`'s Z
mapping is linear, a fixed NDC-space bias represents
`shadow_bias * (ls_far - ls_near)` world units — so inflating the range
(as above) directly inflates the world-space peter-panning margin.

Confirmed numerically (`/tmp/.../scratchpad/csm_clip_check.cpp`, standalone,
not part of the build) with a synthetic near cascade (half_depth = 24, per
`GlobalLight.cpp`'s own cascade-0 comment) and a ~140×180-unit flat caster
positioned like the reported road segment: the raw (pre-fix) approach gave a
154-unit Z span; a small, window-sized caster (a vehicle) was unaffected by
the bug either way, confirming the issue is specific to wide/large casters.

## Fix

Added `ClipCasterZRange()` in `src/renderer/ShadowRenderer.cpp` (anonymous
namespace): instead of taking a caster's raw light-space AABB, it clips each
of the box's 6 faces (as light-space quads) against the cascade's 4 XY
half-planes (`min_x`/`max_x`/`min_y`/`max_y`) using Sutherland-Hodgman
polygon clipping (`ClipPolygonAxis()`), then takes the Z range over the
surviving clipped-polygon vertices. This correctly computes the Z extent of
`caster ∩ infinite-XY-window-prism` — i.e. only the part of the caster that's
actually relevant to this cascade — handling both a caster straddling the
window's boundary and a caster whose footprint fully *contains* the window
(the road/ground-slab case, where no single box *edge* crosses the window,
so a cheaper edge-only clip would have missed it — faces are needed).

Re-ran the same synthetic scenario against the fixed logic: the road's Z
span dropped from 154 to ~49 units (down to roughly the physically-expected
size for a caster spanning the full 48-unit-wide window at that tilt angle);
the small vehicle-sized caster's span was unchanged (already fully inside
the window, so clipping is a no-op) — confirms the fix targets exactly the
oversized-caster case without perturbing the common case.

`RenderCascades`'s caster loop now calls `ClipCasterZRange(r->GetWorldBBox(),
b.light_view, b.min_x, b.max_x, b.min_y, b.max_y, &min_z, &max_z)` instead of
transforming the whole bbox and taking its raw min/max.

## Files changed

| File | Change |
|------|--------|
| `src/renderer/ShadowRenderer.cpp` | Added `ClipPolygonAxis`/`ClipCasterZRange`/`kBoxFaces` (anonymous namespace); caster Z-fit loop in `RenderCascades` now clips instead of taking raw bbox Z |

No other files changed; `GlobalLight.{h,cpp}`, `CascadeLightBasis.h`,
`ShadowRenderer.h`, `CSMInfos.h` are untouched by this follow-up.

## Decisions

**Face-clip (Sutherland-Hodgman), not edge-clip or ray-cast**: an
edge-only clip (checking only where the box's 12 edges cross the window
planes) would miss the "window fully inside a face" case exactly
demonstrated by the road/ground scenario — no edge of a huge flat slab
passes through a small window entirely inside its footprint, only its
*faces* do. Sequential half-plane (Sutherland-Hodgman) clipping of each of
the 6 faces subsumes both the edge-crossing case and the face-containment
case in one pass, with no need to special-case either. A world-space
ray-cast-from-window-corners alternative was also considered (using the
existing `BBox3::IntersectsRay`) but requires inverting `light_view` (or
manually reconstructing its basis vectors) and still needs the edge-clip
pass on top for full correctness — more machinery for the same result.

**Stack arrays, not `std::vector`**: `ClipPolygonAxis` writes into
caller-provided fixed 8-`Vec3f` buffers rather than allocating — a convex
polygon starting at 4 vertices (a box face) can gain at most one vertex per
clip against a single plane, and this is called for exactly 4 planes, so 8
is a safe, provably-sufficient upper bound. This runs once per shadow-casting
`Renderable` per cascade per frame; avoiding heap churn in that loop matters
for scenes with many casters.

**Bias left untouched**: considered scaling `shadow_bias` by
`(ls_far - ls_near)` per cascade as a complementary/alternative fix, but that
treats a symptom of an imprecise Z fit rather than the fit itself, and would
have required threading a new per-cascade bias value through `CSMInfos`
(GPU-facing `std140` layout) and the GLSL shader — larger surface area for a
problem the geometric fix above resolves directly at the source.

## Notes for next features

- The remaining ~49-unit Z span for a window-spanning caster (vs. the
  ~6-unit span for a window-sized caster) is not a bug — it reflects the
  caster's actual light-space extent at that light-tilt angle once properly
  clipped to the window, not an overestimate. If depth precision or bias
  tuning ever needs finer control per cascade, a depth-range-aware bias
  (flagged as the rejected alternative above) would be the next lever to
  pull rather than tightening the geometric fit further.
- Still no `renderer/` unit test coverage (see the prior history entry's
  same note). The standalone scratch verification used here for this fix is
  not committed to the repo (per instructions, scratch/temp verification
  artifacts live outside the project tree) — if CSM Z-fit regressions recur,
  formalizing `ClipCasterZRange` as a testable free function (e.g. in a
  dedicated header) would let this kind of check live in `tests/` instead.
- **Visual QA still required as a follow-up** (same headless-environment
  caveat as the prior entry): the numeric check here validates the geometry
  fix in isolation, not the end-to-end visual result in a windowed build.

## Skills used

None (direct implementation, root-caused from user's bug report).

## CLAUDE.md instructions followed

- One class per `.h`/`.cpp` pair: no new files; helpers added to the existing
  `ShadowRenderer.cpp` anonymous namespace, consistent with its existing
  local-constant pattern.
- `cpplint` run and clean on the touched file.
- Did not launch `wreckoning`/`wreckoning_editor` (headless environment) —
  relied on `cpplint`, a full rebuild of both targets, `ctest`, and a
  standalone numeric verification of the new clipping logic against a
  synthetic reproduction of the reported bug scenario.
- History file written per contribution requirement.
