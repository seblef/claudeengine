# ShadowMapPool: Multi-Tier Pre-Allocation, Screen-Size LOD, Hysteresis (Issue #147)

## Changes

### `data/config.yaml`
Added a `shadows:` section with the full pool configuration:
```yaml
shadows:
  2d_pool:
    slots_per_tier: 8
    tiers: [64, 128, 256, 512, 1024]
  cube_pool:
    slots_per_tier: 4
    tiers: [64, 128, 256, 512]
  screen_size_thresholds:
    t64: 20  t128: 50  t256: 100  t512: 200  t1024: 350
    min: 10
  hysteresis_factor: 0.8
```

### `src/core/ShadowConfig.h/.cpp` (new)
`ShadowConfig` class in `core/`, following the `GraphicsConfig` pattern.
- Nested `ShadowPoolConfig` struct with `slots_per_tier` and `tiers`.
- `GetScreenSizeThreshold(int resolution)` — threshold for that tier, `INT_MAX` if unknown.
- `GetMinThreshold()` / `GetHysteresisFactor()` — cutoff and hysteresis multiplier.
- Parses YAML keys `2d_pool`, `cube_pool`, `screen_size_thresholds` (keys `t<res>` + `min`), `hysteresis_factor`.

### `src/core/AppConfig.h/.cpp`
- Added `#include "core/ShadowConfig.h"`.
- Added `static ShadowConfig shadows_` member and `static const ShadowConfig& GetShadows()`.
- `AppConfig::Init()` now calls `shadows_.Parse(root["shadows"])`.

### `src/core/CMakeLists.txt`
Added `ShadowConfig.cpp`.

### `src/renderer/ShadowMapPool.h/.cpp` (new)
Pre-allocates all shadow maps at startup and assigns them per-frame.

**Key private types:**
- `Slot { unique_ptr<ShadowMap> map; const Light* owner; float last_screen_radius; }` — one pre-allocated map.
- `TierSlots { int resolution; vector<Slot> slots; }` — all slots for one resolution tier.

**`BuildPool(video, ShadowPoolConfig)`** (static private) — constructs `vector<TierSlots>` by allocating `slots_per_tier` `ShadowMap`s per resolution.

**`Assign(lights, camera)`** algorithm:
1. Compute `screen_radius` for each shadow-casting local light via `ComputeScreenRadius`.
2. Hysteresis pass: lights currently holding a slot are retained if `radius >= tier_threshold * hysteresis_factor`; otherwise their slot is released.
3. Collect remaining lights above `min_threshold`; sort descending by screen radius.
4. Greedy assignment: for each light, `FindTargetTierIdx` picks the highest-resolution tier whose threshold ≤ screen_radius (capped by `light->GetShadowResolution()`). Tries that tier down to tier 0 for a free slot.

**`ComputeScreenRadius(light, camera)`:**
- Formula: `sphere_radius * screen_height / (2 * tan(fov/2) * dist)`.
- Per light type (via `GetType()` switch):
  - `kOmni`: sphere = `(light_pos, omni->GetRadius())`.
  - `kCircleSpot`: bounding sphere center = `light_pos + dir * (range/2)`, radius = `range / (2 * cos(outer_angle))`.
  - `kRectSpot`: same with `max(h_angle, v_angle)`.

**`GetShadowMap(light)`** — returns assigned `ShadowMap*` or `nullptr` (no shadow this frame).
**`GetAssignedResolution(light)`** — returns map resolution or 0.
**`ClearAll()`** — resets all `owner` pointers and clears assignments.

### `src/renderer/ShadowRenderer.h/.cpp`
- Replaced `unordered_map<Light*, unique_ptr<ShadowMap>> shadow_maps_` with `ShadowMapPool pool_`.
- Constructor: `pool_(video, core::AppConfig::GetShadows())`.
- `RenderShadowMaps()` signature gains `const core::Camera& camera`.
- Start of `RenderShadowMaps`: calls `pool_.Assign(lights, camera)`.
- Per-light allocation replaced by `pool_.GetShadowMap(light)` — skips if `nullptr`.
- `ClearShadowMaps()` delegates to `pool_.ClearAll()`.
- `GetShadowMap()` delegates to `pool_.GetShadowMap()`.
- Range-for uses `const Light*` (cppcheck fix).
- Removed `kDefaultShadowResolution` constant (no longer needed).

### `src/renderer/CMakeLists.txt`
Added `ShadowMapPool.cpp`.

### `src/renderer/Renderer.cpp`
`RenderShadowMaps` call now passes `*camera_` and is guarded by `if (camera_)`.

## Key Decisions

**Cube pool pre-allocated but unused**: The pool allocates cube-map slots at startup but `Assign()` only processes 2D lights (kCircleSpot, kRectSpot). When OmniLight cube-map shadows are added (#150), only `AssignPool` needs extension — no structural change.

**`GetType()` dispatch for screen radius**: Rather than a virtual `GetInfluenceSphereRadius()` on `Light`, the pool uses `GetType()` and static_cast. Adding a new light type requires adding one case in `ComputeScreenRadius`; this is the same callsite where the cube-vs-2D decision lives, keeping shadow-pool logic together.

**`BuildPool` as static private member**: The factory function needs access to the private `TierSlots` struct, so it cannot be a free function in the anonymous namespace. A static private method keeps the struct private while allowing the factory.

**`std::find_if` for free-slot search**: Replaces the raw `break`-loop per cppcheck `useStlAlgorithm` guidance; clearer intent.

**`ShadowConfig` in `core/`**: Follows the existing `GraphicsConfig` pattern. Shadow pool parameters are application-level config, not renderer internals. The renderer includes `core/AppConfig.h` to access them.

**Hysteresis implementation**: A light keeps its current tier as long as `screen_radius >= tier_threshold * hysteresis_factor`. No "upgrade hysteresis" — if a light grows, it gets a better tier on the next assignment cycle. Only downgrades are dampened.

## Notes for Future Issues

- Issue #149 (CSM for GlobalLight): `GlobalLight` already bypasses the pool (`kGlobal` type is skipped in `Assign`). The CSM implementation will add a separate code path in `ShadowRenderer`.
- Issue #150 (OmniLight cube shadows): Add `kOmni` handling in `AssignPool` to use `pool_cube_`, and add a cube shadow-pass in `ShadowRenderer` that iterates `pool_cube_` assignments.
- `kDefaultShadowResolution` constant was removed from `ShadowRenderer.h`; it is no longer needed.
- `config.yaml` `screen_size_thresholds` YAML keys must match `t<resolution>` naming convention for `ShadowConfig::Parse` to recognise them.

## Skills Used
- `impl-issue` skill
