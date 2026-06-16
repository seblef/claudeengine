# Particle Color Intensity — Issue #577

## Summary

Added start/end color intensity multipliers to particle sub-systems, enabling
HDR-style output for effects like fire where RGB values should exceed 1.0 to
drive bloom and tone-mapping pipelines.

## Changes

### `src/particles/ParticleSubSystemDesc.h`
Added two new fields after the color gradient block:
```cpp
float intensity_start = 1.f;
float intensity_end   = 1.f;
```
Both default to `1.f` — fully backward-compatible with all existing data files.
The comment documents that alpha is intentionally unaffected.

### `src/particles/ParticleEmitter.cpp`
Two touch-points:

1. **`Update()` integration loop** — after `SampleGradient()`, the interpolated
   intensity `lerp(intensity_start, intensity_end, t)` is applied to `r`, `g`, `b`
   only:
   ```cpp
   const float intensity = desc_.intensity_start +
                           (desc_.intensity_end - desc_.intensity_start) * t;
   p.color.r *= intensity;
   p.color.g *= intensity;
   p.color.b *= intensity;
   ```

2. **`SpawnParticle()`** — the initial color (at age 0) is scaled by
   `intensity_start` so newly born particles are already at the correct brightness.

### `src/particles/ParticleSystemTemplate.cpp`
`ParseSubSystem()` reads two optional keys `intensity_start` and `intensity_end`
(both default to `1.f` when absent).

### `src/editor/ParticleEditorWindow.cpp`
- **`SerializeToFile()`** — emits `intensity_start:` and `intensity_end:` for
  every sub-system.
- **`RenderSubSystemProperties()`** — adds a `DragFloat2("Intensity (start/end)")`
  widget in the Color gradient section, with range `[0, 20]` and step `0.05`.
  Values above 1 are valid and expected for HDR fire/glow effects.

## Decisions

- **RGB-only multiplication** — intensity is luminosity, not opacity.  Alpha
  already handles transparency through gradient stops; mixing intensity into alpha
  would produce unexpected fades when intensity_end is low.
- **`[0, 20]` UI clamp** — wide enough for typical HDR fire (3–6 ×) and plasma
  (10–15 ×) without letting the slider run off to unusable values.  Users can
  type arbitrary values if needed.
- **No per-stop intensity** — keeping intensity as two scalar floats (start/end)
  avoids changing `ColorStop` and breaking the serialized format.  A per-stop
  approach could be added later; the start/end pair covers the main use cases.
- **Backward compatibility** — defaults of `1.f` mean every existing
  `.particles.yaml` without these keys renders identically to before.

## Skills / CLAUDE.md rules used

- `src/CLAUDE.md`: Google C++ style, `src/` include root.
- `src/particles/CLAUDE.md`: dependency graph, one struct per header.
- `src/editor/CLAUDE.md`: panels must be pure UI; no logic in Render methods.
- Global CLAUDE.md: conventional commits, history file, cpplint before commit.
