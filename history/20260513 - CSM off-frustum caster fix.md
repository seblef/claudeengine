# CSM off-frustum shadow caster fix

**Branch:** `feat/demo-floor-shadow-scene`  
**Commit:** `e5d8652`

## Problem

Shadow casters outside the camera view frustum were silently dropped from the
cascade shadow maps.  The root cause was in `GlobalLight::ComputeCascadeMatrices()`:
`ls_near` was computed from the closest cascade-frustum corner in light view:

```cpp
const float ls_near = std::max(kMinNear, -max_z);
```

Where `max_z` is the maximum (least negative) light-view Z of any camera-frustum
corner.  Any object with light-view Z *greater* than `max_z` (i.e. between the
light source and the cascade's near plane) is geometrically clipped by the ortho
projection.  This silently excluded:

* Tall geometry whose bounding box top is above the camera frustum.
* Objects *behind* the camera that are closer to the directional light than the
  nearest cascade corner.
* Any mesh that strays slightly outside the field of view while still casting a
  visible shadow onto floor or nearby receivers.

The same VP matrix is used for both shadow-map rendering (via `ShadowPassInfos`)
and shadow-caster frustum culling (`ViewFrustum(cascade_vp)`), so a tight
`ls_near` both culls the object from the CPU list *and* Z-clips it on the GPU.

## Fix

Set `ls_near = kMinNear` (0.1) unconditionally:

```cpp
const float ls_near = kMinNear;          // was: std::max(kMinNear, -max_z)
const float ls_far  = -min_z + half_depth;  // unchanged
```

Since the directional-light projection is orthographic, the shadow UV coordinates
(XY in clip space) are independent of the Z range.  Making the near plane smaller
only changes depth-buffer precision slightly—acceptable given that each cascade
already covers a small depth slice.

`max_z` is now unused and was removed from the per-corner loop.

## Rationale

The XY spherical fit already ensures that receivers in the camera frustum are
covered.  For a caster to project a shadow onto any of those receivers it must
fall within the same ±half_depth XY window—so the XY extent is already correct.
The only additional requirement is that the caster's Z range be included, which
is now guaranteed by starting from the light source itself (ls_near = 0.1).

## Files changed

| File | Change |
|------|--------|
| `src/renderer/GlobalLight.cpp` | `ls_near = kMinNear`; removed `max_z` from loop |

## Skills used

None (continuation from compacted session).

## Notes for next features

* `ls_far = -min_z + half_depth` still assumes casters are at most `half_depth`
  beyond the cascade's farthest corner in light view.  Very large scenes may
  need this extended to `camera.GetMaxDepth()` for the far cascades.
* If depth precision issues appear (shadow acne), tuning `shadow_bias` per
  cascade or adding a slope-scale bias would be the next step.
