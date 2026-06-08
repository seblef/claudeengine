# Bloom Mip-Chain Renderer

**Date:** 2026-06-08  
**Issue:** #412  
**Branch:** feat/bloom-mip-chain-renderer

---

## Summary

Implemented a physically-motivated bloom effect using a dual mip-chain downsample / upsample approach (popularised by Epic / Call of Duty Advanced Warfare). The feature introduces `renderer::BloomRenderer` and four GLSL shader programs in `data/shaders/glsl/bloom/`.

---

## Files Changed

| File | Change |
|------|--------|
| `src/renderer/BloomRenderer.h` | New class declaration |
| `src/renderer/BloomRenderer.cpp` | New class implementation |
| `src/renderer/CMakeLists.txt` | Added `BloomRenderer.cpp` |
| `data/shaders/glsl/bloom/bloom_downsample_vs.glsl` | Fullscreen-quad vertex shader |
| `data/shaders/glsl/bloom/bloom_downsample_ps.glsl` | 13-tap Kawase box filter + threshold |
| `data/shaders/glsl/bloom/bloom_upsample_vs.glsl` | Fullscreen-quad vertex shader |
| `data/shaders/glsl/bloom/bloom_upsample_ps.glsl` | 9-tap tent filter upsample |

---

## Algorithm

### Downsample pass (6 levels, each half the previous)

1. **Level 0:** HDR buffer → levels_[0] (W/2 × H/2) with luminance threshold:
   `color = max(color - u_threshold, 0)` — extracts only bright pixels.
2. **Levels 1–5:** plain 13-tap Kawase-style box filter (`u_threshold == 0`).

The 13-tap filter uses a weighted star pattern (weights sum to 1.0):
- Inner box corners `{d,e,i,j}`: 0.125 each
- Edge midpoints `{b,f,h,l}`: 0.0625 each
- Center `g`: 0.125
- Outer corners `{a,c,k,m}`: 0.03125 each

### Upsample pass (levels 5→0)

Starting from the smallest level, each level is additively blended with the upsampled result from the level below using a **9-tap tent filter** (weights sum to 1.0):

```
framebuffer[i] = levels[i]_base + tent9(levels[i+1]) * u_intensity
```

### Texture feedback loop avoidance

A key implementation decision: the upsample writes to `levels_[i]` while needing to incorporate the existing content of `levels_[i]` (the "base"). Instead of reading and writing the same RT (undefined behavior / GL feedback loop), we use **GPU additive blending**:

- The upsample shader outputs only `tent9(u_src) * u_intensity`
- The GPU's additive blend mode accumulates this into `levels_[i]`'s existing content
- Only `levels_[i+1]` is sampled; `levels_[i]` is never a simultaneous read+write target

This is the standard approach in Unreal/Unity URP and avoids extra ping-pong buffers.

---

## Render Target Formats

All 6 levels use `kG11R11B10F` (R11F_G11F_B10F in OpenGL):
- Lower bandwidth than RGBA16F (no alpha channel needed for bloom)
- Sufficient dynamic range for bright-light accumulation

A 1×1 `kG11R11B10F` "null" RT is allocated for the no-op path when `intensity == 0`.

---

## API

```cpp
BloomRenderer bloom;
bloom.Create(video, width, height);

// Per frame:
abstract::RenderTarget* bloom_rt = bloom.Render(
    emissive_fbo.GetHDRRT(),
    post_process_infos.bloom_threshold,  // e.g. 1.0
    post_process_infos.bloom_intensity   // e.g. 0.8; 0 = no-op
);
// bloom_rt can be bound as sampler for the composite pass

// On window resize:
bloom.Resize(new_w, new_h);

// On shutdown:
bloom.Destroy();
```

---

## What's Next (Issue #412 dependency chain)

- **Issue #413 / wiring**: Integrate `BloomRenderer` into `Renderer::Update()` between the water pass (step 4b) and the composite pass (step 5). Read `PostProcessConfig::IsBloomEnabled()` before calling `Render()`.
- **Composite shader**: Add `layout(binding=1) uniform sampler2D u_bloom` and blend it additively over the HDR buffer before tone-mapping.
- **PostProcessInfos UBO**: `u_bloom_threshold` and `u_bloom_intensity` are already present in slot 10 — no changes needed.

---

## Skills / Instructions Referenced

- `impl-issue` skill
- `src/CLAUDE.md`: one class per file, include root is `src/`, Google C++ style
- `data/shaders/glsl/CLAUDE.md`: shader naming `{name}_vs.glsl` / `{name}_ps.glsl`, `#include` for uniforms/layouts
- Global `CLAUDE.md`: conventional commits, history file, cpplint required
