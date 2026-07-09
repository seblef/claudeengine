# CSM caster-aware near/far fit

**Branch:** `fix/csm-caster-aware-near-far`

## Problem

Cascaded shadow maps for `GlobalLight` were clipping parts of shadow-casting
meshes — reported via the debug panel showing suspicious z-near/z-far values,
confirmed visually as meshes not fully casting shadows.

`GlobalLight::ComputeCascadeMatrices()` sized each cascade's orthographic
`ls_near`/`ls_far` purely from the 8 corners of the camera's own sub-frustum
for that cascade (`half_depth`, the corners' bounding-sphere radius) — never
from the actual shadow casters in the scene:

- `ls_near = kMinNear` (0.1, hardcoded). A prior fix
  (`history/20260513 - CSM off-frustum caster fix.md`) already loosened this
  from a corner-derived value to a flat constant, but it still couldn't help
  a caster sitting farther "sunward" than the light eye's own push-back
  distance (`half_depth`) — such a caster ends up geometrically *behind* the
  eye (positive light-view-space Z), which no near-plane value alone can fix.
- `ls_far = -min_z + half_depth` — extended the far plane by `half_depth`
  past the farthest receiver corner, a heuristic margin. The same history
  file already flagged this as unresolved: *"very large scenes may need this
  extended... for far cascades."*

Both were symptoms of the same gap: the light-space Z range was sized to the
*receiver* frustum, never to what's actually casting shadows into it.

## Fix

Since the light is directional (orthographic), a caster can only shadow a
receiver if it shares the receiver's light-space X/Y footprint (only Z
differs along the parallel light rays) — so the existing X/Y fit (a
conservative spherical fit around `half_depth`) is already provably
sufficient and was left untouched. Only Z needed fixing.

Implemented a two-pass, caster-aware Z fit, per the user's explicit request
to derive near/far from "an infinite parallelepiped derived from the camera
frustum":

1. `GlobalLight::ComputeCascadeMatrices` → renamed
   `GlobalLight::ComputeCascadeBasis`. For each cascade it now outputs a
   `CascadeLightBasis` (new header, `src/renderer/CascadeLightBasis.h`) —
   light view matrix, X/Y bounds, and the receiver frustum's own Z range
   (`receiver_min_z`/`receiver_max_z`, now tracking both min *and* max,
   previously only min) — instead of building the final VP itself.
   `GlobalLight` has no visibility into scene casters, so it can no longer
   own the final near/far fit.

2. `ShadowRenderer::RenderCascades` (which already has `no_cull`/`octree` in
   scope) does the caster-aware fit per cascade:
   - Builds a "probe" orthographic frustum sharing the cascade's X/Y bounds
     but with a deliberately generous Z range (`±kCSMProbeZHalfExtent`,
     5000 units) — exploiting that `OrthoOffCenterRH`'s left/right/top/bottom
     clip planes don't depend on z_near/z_far at all, so any finite range
     works for the probe's side planes.
   - Queries `no_cull`/`octree` with that probe frustum to find every
     candidate caster overlapping the receiver's XY footprint, regardless of
     depth — effectively an "infinite parallelepiped" extruded from the
     cascade.
   - Unions the receiver's own Z range with each candidate caster's
     light-space AABB (`Renderable::GetWorldBBox() * light_view`, using the
     existing `BBox3::operator*(Mat4f)` which transforms all 8 corners in one
     call), skipping non-caster renderables via `IsShadowCaster()` (so
     `GlobalLight` itself, `always_visible` with `BBox3::kInfinite`, doesn't
     poison the fit).
   - Derives `ls_near`/`ls_far` directly from that tight union plus a small
     epsilon (`kCSMZFitEpsilon`, 0.05), builds the final `ortho * light_view`.
   - Reuses the probe-collected caster list for the actual depth-pass render
     instead of re-querying with the final (tighter) frustum — valid because
     the final frustum shares the probe's exact X/Y planes and its Z range
     was derived directly from these same casters, so a second query would
     return an identical set. This keeps octree query cost unchanged (one
     `CullAndCollect` pair per cascade, same as before the fix).

`ls_near` can legitimately go non-positive now (the light "eye" is just an
arbitrary reference frame for building `light_view`, not a hard near-plane
origin) — confirmed safe since `OrthoOffCenterRH` only requires
`z_near != z_far`, no positivity constraint.

## Files changed

| File | Change |
|------|--------|
| `src/renderer/CascadeLightBasis.h` (new) | Header-only struct: per-cascade light view + XY bounds + receiver Z range |
| `src/renderer/GlobalLight.h` | Renamed `ComputeCascadeMatrices` → `ComputeCascadeBasis`; new signature takes `CascadeLightBasis*` output array instead of directly filling `CSMInfos::cascade_vp` |
| `src/renderer/GlobalLight.cpp` | Same rename; corner loop now tracks `max_z` in addition to `min_z`; removed `kMinNear`/`ls_near`/`ls_far`/`OrthoOffCenterRH` call (moved to `ShadowRenderer`) |
| `src/renderer/ShadowRenderer.cpp` | `RenderCascades` rewritten: probe-frustum caster query, tight Z union, final VP build, caster-list reuse; two new constants (`kCSMProbeZHalfExtent`, `kCSMZFitEpsilon`) |

`src/renderer/ShadowRenderer.h` and `src/renderer/CSMInfos.h` needed no
changes — `RenderCascades`'s private signature already took `no_cull`/
`octree`, and `CSMInfos`'s GPU-facing layout is untouched.

## Decisions

**Kept the caster-query logic out of `GlobalLight`**: every other light type
in this codebase keeps scene-content queries out of the `Light` hierarchy
(`Light::ComputeShadowVP()` is pure geometry; only `ShadowRenderer` calls
`IVisibilitySystem::CullAndCollect`). Adding `no_cull`/`octree` params
directly to `GlobalLight::ComputeCascadeMatrices` would have made
`GlobalLight` the only light type depending on `IVisibilitySystem`/
`Renderable`, for no benefit — `ShadowRenderer` already owns 100% of the
"query casters, build frustum, render depth" orchestration for spot lights
and omni cube maps.

**Reused the probe caster list for rendering** rather than re-running
`CullAndCollect` with the final tightened frustum: provably redundant given
the final frustum's X/Y planes are identical to the probe's and its Z range
was derived directly from the probe's own results — avoids doubling octree
query cost per cascade.

**Probe Z half-extent (5000) is a new constant, not `kMaxShadowDistance`
(150)**: the two are unrelated. `kMaxShadowDistance` bounds *cascade
slicing* along the camera's view axis; the probe's Z range bounds *caster
search* along the light axis and has zero rendering-precision cost (the
probe matrix is discarded after the query), so it can and should be
generous.

## Notes for next features

- A shadow-caster `Renderable` with a very large or `BBox3::kInfinite` world
  bbox would still blow out the tight Z fit for any cascade it's collected
  into. No such caster exists today (the only `kInfinite`-bbox renderable,
  `GlobalLight` itself, is excluded by the `IsShadowCaster()` guard). Inherent
  limit of any bbox-based tight fit — not introduced by this change, flagged
  for awareness only, same as the prior fix's own residual-limitation note.
- **Visual QA required as a follow-up**: this is a shadow-rendering geometry
  change with no way to visually confirm correctness in this headless
  environment (no display, per `CLAUDE.md` cannot launch `wreckoning`/
  `wreckoning_editor`). Needs manual verification in a windowed build that
  (a) meshes no longer show clipped/incomplete shadows across all 4 cascades,
  and (b) no new shadow acne or z-fighting was introduced by the
  occasionally-tighter or occasionally-looser near/far range (32-bit float
  depth buffer should make this a non-issue, but unverified visually).
- No `renderer/` unit test directory exists yet (`tests/` only covers
  `core`, `game`, `mesh`, `vfx`). This change has no automated geometry
  regression coverage beyond a full rebuild + the manual hand-derivation
  done during planning; adding a `GlobalLight`/`ShadowRenderer` test target
  would be a reasonable follow-up if CSM correctness issues recur.

## Skills used

None (direct implementation from an approved plan).

## CLAUDE.md instructions followed

- One class per `.h`/`.cpp` pair: `CascadeLightBasis` is header-only (POD
  struct), matching the existing `CSMInfos.h` precedent.
- Include paths project-relative from `src/`.
- `cpplint` run and clean on all 4 touched/added files.
- Did not launch `wreckoning`/`wreckoning_editor` (headless environment) —
  relied on `cpplint`, a full rebuild of both targets, and `ctest` instead;
  flagged visual QA as a follow-up above per the "Verification" guideline.
- Conventional commit message format.
- History file written per contribution requirement.
