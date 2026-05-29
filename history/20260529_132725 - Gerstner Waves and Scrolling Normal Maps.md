# Gerstner Waves and Scrolling Normal Maps

**Issue:** #353
**Branch:** feat/environment-gerstner-waves-353
**Date:** 2026-05-29

## Changes

### `data/shaders/glsl/water/water_vs.glsl`

Replaced the 4 sine-wave vertex displacement with Gerstner waves. Gerstner waves model physically realistic circular particle orbits that produce wave cresting (the lateral XZ offset pulls particles toward the crest, unlike pure sine which only moves them vertically).

Key changes:
- Each wave now computes `phi_i = freq_i × dot(dir_i, xz) + speed_i × time` and applies `(Q × A × dir.x × cos(phi), A × sin(phi), Q × A × dir.y × cos(phi))` offsets.
- Steepness parameter `Q = 0.5` chosen as a good trade-off: visible cresting without self-intersection (which occurs at Q = 1).
- Full analytical TBN frame (tangent T, bitangent B, normal N) derived from the Gerstner position derivatives, replacing the simple finite-difference normal. This is required to correctly rotate the normal-map tangent-space normals into world space.
- Added `out vec3 v_tangent`, `out vec3 v_bitangent` outputs.
- Added `out vec2 v_uv = in_position.xz` (world XZ coordinates for tileable normal map sampling — scale-independent).

### `data/shaders/glsl/water/water_ps.glsl`

Added two-layer scrolling normal map sampling:
- Two texture samples of `u_normal_map` (sampler slot 0) at UV scales `foam_params.z/w` and scroll speeds `scroll_params.x/y`, with slightly different scroll directions to break repetition.
- Tangent-space normals decoded from `RGB * 2 − 1` and blended via additive normalization (`normalize(n1 + n2)`).
- Result transformed to world space using `TBN = mat3(v_tangent, v_bitangent, v_world_normal)`.
- This replaces the previous use of the Gerstner vertex normal directly, giving micro-surface ripple detail that is independent of the vertex grid resolution.

### `src/environment/WaterRenderer.h`

- Added `#include "abstract/RawTexture.h"`.
- Added `std::unique_ptr<abstract::RawTexture> normal_map_tex_` member.
- Declared `BuildNormalMap()` private method.

### `src/environment/WaterRenderer.cpp`

- Added `#include <cmath>` and `#include <vector>` (needed for normal map generation).
- `BuildNormalMap()`: Generates a 512×512 RGBA8 tileable water normal map using a height field of four overlapping multi-frequency sine waves. Finite differences (`eps = 1` texel) compute the surface gradient; the gradient is scaled by `8.0` to give enough contrast. Normal encoded as `R = N.x·0.5+0.5, G = N.z·0.5+0.5, B = N.y·0.5+0.5` to match the standard tangent-space normal map convention expected by the shader.
- `Build()`: calls `BuildNormalMap()` after `BuildMesh()`.
- `Render()`: binds `normal_map_tex_` to slot 0 before the draw call.
- `Reset()`: calls `normal_map_tex_.reset()`.

## Decisions and Rationale

**Why Gerstner waves?** Sine waves only displace vertices vertically, making them physically inaccurate (water particles move in circles, not straight up/down). Gerstner waves add the lateral XZ displacement that creates cresting, a significant visual improvement at low computational cost.

**Why Q = 0.5?** The steepness parameter controls the ratio of lateral to vertical displacement. At Q = 0 the wave is a pure sine; at Q = 1 the crest becomes a cusp (and meshes self-intersect). Q = 0.5 gives visible cresting without artifacts.

**Why analytical TBN?** The fragment shader needs to rotate normal-map tangent-space normals into world space. Computing TBN analytically from the Gerstner derivatives is exact and cheap in the vertex shader. The alternative (computing finite differences in the PS) would be far more expensive.

**Why `v_uv = in_position.xz` (world XZ)?** Using world coordinates for normal map UV ensures the normal map tiles at a world-space scale that is independent of the grid resolution. The scale factors `foam_params.z/w` in the PS are in world-space units per tile, making them intuitive to tune.

**Why procedural normal map generation?** Avoids an external asset dependency, keeps everything self-contained, and guarantees perfect tileability. The four sine waves at coprime frequencies produce a non-repeating, organic ripple pattern.

**Normal map encoding convention:** `R = N.x·0.5+0.5, G = N.z·0.5+0.5, B = N.y·0.5+0.5`. Note: `G` encodes `N.z` (not `N.y`), consistent with the shader decoding `texture().rgb * 2 - 1` and using all three channels in the TBN rotation. The `B` channel holds the "up" component which is always positive.

## Output for future features

- The `WaterInfos` struct already has `normal_scale1/2` and `normal_scroll_speed1/2` fields — these are exposed to the editor panel via the existing `WaterInfos` upload path in `Renderer`.
- Future work could replace the procedural normal map with a loaded tileable texture asset for higher quality detail.
- The Gerstner `Q_STEEP` constant is currently hardcoded in the shader; it could be moved to `WaterInfos` as a runtime parameter for editor tuning.

## Skills and files used

- **Skills:** impl-issue
- **CLAUDE.md rules followed:** branch naming `feat/`, conventional commit message, Google C++ style, single class per file, project-relative includes, build in `_build/`, history file in `history/`.
