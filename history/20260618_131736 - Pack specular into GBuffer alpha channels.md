# Pack specular data into G-buffer alpha channels (eliminate RT2)

## Summary

Eliminated the dedicated specular render target (RT2) by packing specular data into the
unused alpha channels of the two remaining colour render targets:

| Slot | Format  | Before                         | After                                           |
|------|---------|--------------------------------|-------------------------------------------------|
| RT0  | RGBA8   | rgb=diffuse, **a=0 (wasted)**  | rgb=diffuse, **a=spec_intensity**               |
| RT1  | RGBA16F | rgb=normal, **a=0 (wasted)**   | rgb=normal, **a=shininess/256**                 |
| RT2  | RGBA8   | r=spec_intensity, g=shin/256   | **removed**                                     |

## Changes

### G-buffer pixel shaders (geometry-pass writers)
All five shaders (`gbuffer_ps.glsl`, `terrain_ps.glsl`, `foliage_ps.glsl`, `tree_ps.glsl`,
`particle_gbuffer_ps.glsl`) were updated to:
- Remove `layout(location = 2) out vec4 out_specular`
- Pack `spec_intensity` into `out_albedo.a`
- Pack `shininess / 256.0` into `out_normal.a`
- Update header comment blocks

### Lighting pass shaders (G-buffer readers)
Both `global_light_ps.glsl` and `omni_light_ps.glsl`:
- Removed `layout(binding = 7) uniform sampler2D u_specular`
- Replaced specular sampling with `albedo_s.a` / `normal_s.a * 256.0`
- Reused the already-fetched texel for efficiency (one texture fetch per RT)

### Debug blit shader
`debug_blit_ps.glsl` mode 3 (`kSpecular`): updated to display `s.a` (spec_intensity from
the albedo RT) as greyscale, since the C++ side now binds the albedo RT for this mode.

### C++ — GBuffer
- `GBuffer.h`: removed `specular_rt_` member and `GetSpecularRT()` accessor; updated doc
  comment (3 RTs → 2 colour RTs); updated `BindForReading` comment
- `GBuffer.cpp`: removed specular RT allocation and reset; reduced `colors` array from 3
  to 2 entries; removed `GetSpecularRT()` implementation

### C++ — Renderer
- `Renderer.cpp`: `kSpecular` debug bypass now binds the albedo RT (`GetAlbedoRT()`);
  removed stale `video_->UnbindSampler(7)` call; updated lighting-pass comment
- `PreviewRenderer.cpp`: updated lighting-pass comment

## Decisions and rationale

- **Precision**: `shininess/256` moves from RGBA8 (8-bit) to RGBA16F (16-bit float), which
  is a precision improvement at no cost.
- **spec_intensity** keeps its 8-bit RGBA8 encoding, so no regression there.
- **No intermediate variables introduced**: lighting shaders store the full `vec4` from
  each texture fetch (`albedo_s`, `normal_s`) and read `.a` directly — avoids a redundant
  second sample.
- **Debug mode 3**: binding the albedo RT (which carries spec in `.a`) and reading `.a` is
  the cleanest approach; it avoids any special-case in the shader beyond the comment update.

## Benefits

- One fewer RT allocated each frame: ~8 MB saved at 1080p (1920×1080 × 4 bytes RGBA8)
- Reduced MRT write bandwidth in the geometry pass
- No precision regression; shininess precision improved

## Output to keep in mind

- `GBuffer::BindForReading(first_slot)` now binds **2** colour samplers: albedo at
  `first_slot`, normal at `first_slot+1`. Any future lighting shader must not declare
  a sampler at `first_slot+2` expecting a specular RT — it no longer exists.
- The `u_specular` sampler name is gone from all shaders; search for it if adding new
  lighting variants.

## Skills / instructions used

- `impl-issue` skill
- `CLAUDE.md`: git workflow (checkout dev → branch → implement → cpplint → commit → PR)
- `src/CLAUDE.md`: Google C++ style, one class per file, meaningful documentation
- `data/shaders/glsl/CLAUDE.md`: GLSL coding style, `#include` usage, equation comments
