# LightInfos Shadow Fields: light_vp, cast_shadow, shadow_bias (Issue #148)

## Changes

### `src/renderer/LightInfos.h`
Extended from 80 B to 160 B:
- Renamed `pad_` → `cast_shadow` (float at offset 76) — `1.0` when a pool slot is assigned, `0.0` otherwise.
- Appended `light_vp` (core::Mat4f, 64 B, offset 80) — light-space view-projection matrix.
- Appended `shadow_bias` (float, offset 144) + `pad1_`, `pad2_`, `pad3_` (alignment to 160 B).
- Added `#include "core/Mat4f.h"`.
- Updated all static_asserts: `sizeof == 160`, `cast_shadow == 76`, `light_vp == 80`, `shadow_bias == 144`.

`light_vp` is placed at offset 80 = 5 × 16, satisfying std140 mat4 alignment (16 bytes). `Mat4f` is `alignas(16)`, so the compiler places it at offset 80 naturally. `sizeof(LightInfos) == 160` is a multiple of 16 — no struct-tail padding needed.

### `data/shaders/glsl/uniforms/light_infos.glsl`
Updated to match the new layout:
- Replaced `float pad_` with `float cast_shadow`.
- Added `layout(row_major) mat4 light_vp` — `row_major` is required so GLSL stores/reads the matrix in row-major order, matching `core::Mat4f`'s storage.
- Added `float shadow_bias` and `float pad1_`, `pad2_`, `pad3_` to fill out 160 bytes.

### `src/renderer/Light.h`
- Added `shadow_bias_` (float, default `0.005f`) with `GetShadowBias()` / `SetShadowBias(float)`.

### `src/renderer/LightRenderer.cpp`
- Added `#include "renderer/ShadowMap.h"` and `#include "renderer/ShadowRenderer.h"`.
- Updated `kLightInfosFloat4s` comment to `160 / 16 = 10`.
- `FillInfos` gains a `const ShadowMap* smap` parameter:
  - Sets `cast_shadow = smap ? 1.0f : 0.0f`.
  - Sets `light_vp = smap->GetLightVP()` when shadow map is assigned (zero otherwise, which is fine since the shader multiplies by `cast_shadow`).
  - Sets `shadow_bias = light.GetShadowBias()`.
- Both call sites (`RenderGlobalLights` and `RenderLocalLights`) now pass `ShadowRenderer::Instance().GetShadowMap(light)` to `FillInfos`.

### `src/game/GameLightDesc.h`
- Added `shadow_bias` float field (default `0.005f`).

### `src/game/GameLight.cpp`
- Added `light_->SetShadowBias(desc.shadow_bias)` after the existing shadow flag forwarding.

## Key Decisions

**`cast_shadow` as float, not bool**: Allows `shadow_factor * cast_shadow` in the shader without a branch — when `cast_shadow == 0.0`, the multiplication collapses the shadow contribution to zero.

**`light_vp` only set when shadow map is assigned**: Zero-initialization from `*infos = {}` sets `light_vp` to a zero matrix when `cast_shadow == 0.0`. This is safe because the shader multiplies by `cast_shadow` before using `light_vp`.

**`FillInfos` takes `const ShadowMap*`**: Decouples the anonymous-namespace helper from `ShadowRenderer`. The caller does the singleton lookup and passes the result in — the helper stays testable and free of global-state dependencies.

**Offsets**: `light_vp` at 80 (5 × 16) satisfies std140 mat4 alignment. `shadow_bias` + 3 pads fill the final 16-byte chunk to reach 160 B total.

## Notes for Future Issues

- Issue #149 (CSM GlobalLight): GlobalLight will get its own shadow mechanism; `cast_shadow` in LightInfos will remain `0.0` for GlobalLight until CSM is wired.
- Shaders consuming `LightInfosBlock` that want to apply shadows must check `cast_shadow` before sampling the shadow map, or simply multiply the shadow factor by `cast_shadow`.
- `shadow_bias` should be tunable per-light; values around `0.001–0.01` work for most scenarios.

## Skills Used
- `impl-issue` skill
