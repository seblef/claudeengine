# Shadow Casting Flags (Issue #146)

## Changes

### `src/renderer/Light.h`
Added two new properties:
- `cast_shadow_` (bool, default `true`) + `GetCastShadow()` / `SetCastShadow(bool)` — when false the light never allocates or renders a shadow map.
- `shadow_resolution_` (int, default `1024`) + `GetShadowResolution()` / `SetShadowResolution(int)` — maximum resolution cap; the pool issue (#147) may assign a lower value based on screen-space size.

No new constructor parameter — both are post-construction settings, consistent with the issue spec and the existing color/intensity pattern.

### `src/renderer/Material.h/.cpp`
Added `cast_shadow_` bool member (default `true`) and `GetCastShadow() const` accessor.
- Loaded from YAML key `cast_shadow` in `LoadFromYaml()`.
- Propagated from `MaterialDesc::GetCastShadow()` in the `Material(const MaterialDesc&, ...)` constructor.

### `src/renderer/MaterialDesc.h`
Added `cast_shadow_` bool field (default `true`) with fluent setter `SetCastShadow(bool) → MaterialDesc&` and `GetCastShadow() const`.

### `src/game/GameLightDesc.h`
Added two fields:
- `cast_shadow` (bool, default `true`) — maps directly to `Light::SetCastShadow`.
- `shadow_resolution` (int, default `1024`) — maps to `Light::SetShadowResolution`.

Existing fields are unchanged.

### `src/game/GameLight.cpp`
After creating the renderer light in the constructor body, calls `light_->SetCastShadow(desc.cast_shadow)` and `light_->SetShadowResolution(desc.shadow_resolution)`. These are post-construction setters to avoid changing all Light subclass constructors.

### `src/renderer/MeshInstance.h`
`IsShadowCaster()` now delegates to `model_->GetMaterial()->GetCastShadow()` instead of returning `true` unconditionally. This was the placeholder left by issue #145 with the comment "Updated to delegate to material->GetCastShadow() in issue #146".

### `src/renderer/ShadowRenderer.cpp`
Two changes to `RenderShadowMaps`:
1. Early-exit check `if (!light->GetCastShadow()) continue;` before `ComputeShadowVP()`, so disabled lights are skipped entirely.
2. Shadow map allocation now uses `light->GetShadowResolution()` instead of the hardcoded `kDefaultShadowResolution`, so per-light caps are respected from the start.

## Key Decisions

**No constructor parameter on Light**: Adding a constructor parameter to `Light` would require updating all subclass constructors and all call sites. Post-construction setters are idiomatic for opt-in flags that most callers won't need to change.

**`cast_shadow` defaults to `true`**: Consistent with the milestone requirement "both flags default to true".

**Per-light resolution stored on Light, not GameLight**: The shadow renderer needs the resolution at map allocation time; passing it through `Light` avoids any coupling between `ShadowRenderer` and the game layer.

**Shadow resolution replaces `kDefaultShadowResolution` at allocation**: The constant `kDefaultShadowResolution` in `ShadowRenderer.h` is now effectively unused for actual allocation (the per-light value is used instead). It can be removed in a later cleanup once the pool is implemented.

## Notes for Future Issues
- Issue #147 (shadow pool): `light->GetShadowResolution()` is the cap — the pool may allocate a lower tier.
- Material YAML format now supports `cast_shadow: false` at the top level.

## Skills Used
- `impl-issue` skill
