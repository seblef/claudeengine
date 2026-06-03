# Water Distance-Based Shader LOD

## Summary

Implements per-fragment XZ distance gating in the water fragment shader so that
expensive visual features are skipped for fragments the player cannot
meaningfully distinguish from a distance.

## Changes

### `src/environment/WaterInfos.h`
- Renamed `wi_pad0_` → `lod_near_dist` (default 50 m): full-quality threshold.
- Renamed `wi_pad1_` → `lod_far_dist` (default 100 m): minimal-quality threshold.
- Updated the offset table comment at the top of the struct.
- `static_assert(sizeof(WaterInfos) == 96)` continues to pass — no layout change.

### `src/environment/WaterRenderer.h`
- Added private members `lod_near_dist_` / `lod_far_dist_` (defaults mirror WaterInfos).
- Added `SetLodNearDist` / `SetLodFarDist` mutators and matching `Get*` accessors so
  callers (editor, game) can tune thresholds at runtime without touching the shader.

### `src/renderer/Renderer.cpp`
- `UpdateWaterRenderer()` now packs `wi.lod_near_dist` and `wi.lod_far_dist` from the
  `WaterRenderer` getters before uploading the WaterInfos constant buffer.

### `data/shaders/glsl/uniforms/water_infos.glsl`
- Updated `scroll_params` comment: `.z = lod_near_dist, .w = lod_far_dist`.

### `data/shaders/glsl/water/water_ps.glsl`
Three LOD tiers derived at the start of `main()`:

```
float frag_dist = length(eye_pos.xz - v_world_pos.xz);
bool  is_near   = frag_dist < scroll_params.z;   // < lod_near_dist
bool  is_mid    = frag_dist < scroll_params.w;   // < lod_far_dist
```

| Feature | Gate | Fallback |
|---|---|---|
| Second normal map sample | `is_mid` | reuse `n1` |
| SSR ray march | `is_near` | `ssr_weight = 0.0` → sky zenith |
| Foam computation | `is_mid` | `foam_amount = 0.0` |
| Wave-tip translucency | `is_mid` | nothing added |

## Decisions

- **`scroll_params.zw` reuse** — the two padding floats were already present in the
  std140 struct (96 bytes). Repurposing them costs no bandwidth and requires no
  layout change.
- **XZ-only distance** — measuring horizontal distance avoids abrupt LOD transitions
  when the camera is elevated (fly-over view) while the water plane is at y = 0.
- **Default thresholds 50 m / 100 m** — chosen to match the issue specification.
  SSR is cut at 50 m because the ray march is the dominant per-fragment cost and
  its contribution is invisible beyond typical camera heights. Foam and translucency
  survive to 100 m because they are cheap relative to SSR.
- **No smooth blending at LOD boundaries** — a hard cut is sufficient because the
  effects are visually negligible exactly where they are disabled. Smooth blending
  would add per-fragment branching overhead that defeats the purpose.

## Output for next features

- `WaterRenderer::SetLodNearDist` / `SetLodFarDist` are already wired through the
  constant buffer — the editor UI panel can expose sliders directly.
- The `is_near` / `is_mid` booleans in the shader are cheap branch-free comparisons;
  future features can be gated at either tier with a single condition.

## Skills and guidelines used

- Followed Google C++ style guide and `src/CLAUDE.md` one-class-per-file rule.
- Used `cppcheck-suppress unusedStructMember` for new private WaterRenderer members.
- Conventional commit message used for the commit.
- Branch created from latest `dev` as required by `CLAUDE.md`.
