# Material Alpha Masking (Cutout Transparency) — #551

## Summary

Added per-material alpha masking: when `alpha_mask: true` is set in a material's YAML, fragments whose diffuse texture alpha is below 0.5 are discarded (`discard`) in both the GBuffer geometry pass and all three shadow depth passes (directional/spot, omnidirectional cube).

## Approach

**Option B — uniform flag in `MaterialInfos`** was chosen over shader permutations. A single `int alpha_mask` flag is appended to the `MaterialInfos` GPU constant buffer (std140). All relevant shaders read this flag and conditionally discard. This avoids any shader selection logic and keeps the shadow renderer pipeline unchanged.

## Files Changed

### C++ — renderer

| File | Change |
|---|---|
| `src/renderer/MaterialInfos.h` | Added `int alpha_mask` at std140 offset 56; updated comment block and static_assert |
| `src/renderer/MaterialDesc.h` | Added `alpha_mask_` bool field + `SetAlphaMask()` / `GetAlphaMask()` builder methods |
| `src/renderer/Material.h` | Added `alpha_mask_` bool field (default `false`) + `GetAlphaMask()` getter |
| `src/renderer/Material.cpp` | Load `alpha_mask` from `MaterialDesc` and from YAML; set `mi.alpha_mask` in `Set()` |
| `src/renderer/MeshRenderer.cpp` | `RenderDepth()` and `RenderDepthCube()` now track the current material and call `mat->Set()` when it changes, so the material CB (including `alpha_mask`) and diffuse texture are up-to-date for the shadow shaders |

### GLSL — shaders

| File | Change |
|---|---|
| `data/shaders/glsl/uniforms/material_infos.glsl` | Added `int alpha_mask` to the UBO block at offset 56 |
| `data/shaders/glsl/geometry/gbuffer_ps.glsl` | Samples full RGBA from diffuse; discards if `alpha_mask != 0 && alpha < 0.5` |
| `data/shaders/glsl/shadow_depth_vs.glsl` | Added `out vec2 v_uv` passthrough |
| `data/shaders/glsl/shadow_depth_ps.glsl` | Includes `material_infos.glsl`; declares `u_diffuse`; discards masked fragments |
| `data/shaders/glsl/shadow_depth_cube_vs.glsl` | Added `out vec2 v_uv` passthrough alongside existing `v_world_pos` |
| `data/shaders/glsl/shadow_depth_cube_ps.glsl` | Same alpha-discard pattern before writing `gl_FragDepth` |

## Decisions & Rationale

- **Single CB flag vs. shader permutation**: The flag approach means one more branch in the shadow PS for all geometry, but this branch is GPU-coherent (uniform value, never diverges within a draw call) and shadow passes are already depth-limited. Zero extra shader permutation maintenance overhead.
- **Threshold hardcoded at 0.5**: Matches the existing pattern in `tree_ps.glsl` and `particle_gbuffer_ps.glsl`. Can be made configurable later if needed.
- **`mat->Set()` in depth pass**: Previously the depth pass bound no material state at all. Now it calls the same `Set()` used by the geometry pass, which binds all 5 textures and fills the CB. Only the diffuse (slot 0) is consumed by the shadow shaders, but binding the unused slots is harmless and keeps the code simple. Non-masked materials incur one extra CB upload per material change in the depth pass — negligible.
- **`_pad` not added explicitly**: `int alpha_mask` at offset 56 leaves 4 bytes of implicit tail padding to reach the 16-byte struct alignment. `sizeof(MaterialInfos)` stays 64 and the existing static_assert is preserved.

## YAML format

```yaml
rendering:
  alpha_mask: true
  textures:
    diffuse: objects/fence.png
```

## Output / Things to Remember

- The depth pass now binds material textures; if profiling shows overhead on scenes with many shadow-casting materials, consider skipping `Set()` when `!mat->GetAlphaMask()` and instead only resetting `alpha_mask` to 0 in the CB. Not done now to keep code simple and correct.
- Submesh support in the depth pass was intentionally left out of scope: `RenderDepth()` still issues one draw call per `MeshInstance` over the full geometry, using the primary material. This is an existing limitation.
- Shadow pass for spot/directional lights uses `shadow_depth_*` shaders; for omni lights it uses `shadow_depth_cube_*` — both are now covered.

## Skills / Instructions Used

- `CLAUDE.md` — branch from `dev`, conventional commits, history file, cpplint
- `data/shaders/glsl/CLAUDE.md` — shader naming conventions, `#include` usage
