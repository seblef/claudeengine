# Particle Fire Extensions — Issue #555

**Branch:** `feat/particle-fire-extensions-555`  
**PR:** #581  
**Date:** 2026-06-16

---

## What was done

Extended the generic particle sub-system with four simulation features needed for convincing fire, smoke and spark effects, without adding specialised system types.

### 1 · Per-particle rotation

`ParticleSubSystemDesc` gained four floats:

```yaml
rotation_start:   { min: 0, max: 360 }   # degrees, random initial angle
angular_velocity: { min: -30, max: 30 }  # degrees/sec, constant spin rate
```

Each `Particle` now stores `angle` (radians, accumulated per frame) and `angular_vel`. The angle is written into the new `VertexParticle::rotation` field and consumed by both vertex shaders, which rotate the billboard corner offsets in the view-space (forward pass) or world-space (g-buffer pass) billboard plane using a 2-D rotation matrix:

```glsl
vec2 rotated_co = vec2(co.x * cos_r - co.y * sin_r,
                       co.x * sin_r + co.y * cos_r);
```

**VertexParticle memory layout** — the new `float rotation` field occupies the 4 bytes that were padding at offset 40 (after `uv_offset`). Total struct size stays 48 bytes; no VBO reallocation or stride change is required. A new vertex attribute at location 4 was added in `GLVertexBuffer.cpp`.

### 2 · Multi-stop colour gradient

Replaced `color_start` / `color_end` (two-stop lerp) with:

```yaml
color_gradient:
  - { key: 0.0, color: [1.0, 1.0, 0.8, 1.0] }
  - { key: 0.3, color: [1.0, 0.55, 0.1, 0.9] }
  - { key: 0.7, color: [0.8, 0.1, 0.0, 0.5] }
  - { key: 1.0, color: [0.0, 0.0, 0.0, 0.0] }
```

`ParticleSubSystemDesc` stores up to `kMaxColorStops = 4` stops as a fixed-size `std::array<ColorStop, 4>` plus a count integer (no heap allocation). The `SampleGradient()` free function in `ParticleEmitter.cpp` iterates the sorted stops and linearly interpolates between the bracketing pair.

**Backward compatibility** — old YAML files that only have `color_start` / `color_end` are auto-converted by `ParticleSystemTemplate.cpp` to a 2-stop gradient (key 0 and key 1). No existing files break. The editor always serialises `color_gradient`.

### 3 · Velocity drag

```yaml
drag: 0.6   # range [0, 1]
```

Applied every frame as:

```cpp
velocity *= (1 - min(drag * dt, 1.f))
```

Simple, branch-free, one float. `drag = 0` (default) is a no-op.

### 4 · Lateral turbulence

```yaml
turbulence_strength:  0.25   # world units/sec amplitude
turbulence_frequency: 2.0    # Hz
```

Each particle gets a random phase offset `turb_phase` at spawn. Per frame:

```cpp
float phase = turbulence_frequency * 2π * age + turb_phase;
position.x += sin(phase) * turbulence_strength * dt;
position.z += cos(phase) * turbulence_strength * dt;
```

The quadrature sin/cos pair (90° offset) produces circular flickering rather than purely axial oscillation.

---

## Key decisions

| Decision | Rationale |
|---|---|
| Fixed `std::array<ColorStop, 4>` instead of `std::vector` | No heap allocation per sub-system; 4 stops is sufficient for all fire / smoke palettes in practice |
| Rotation stored in radians inside `Particle`, converted from degrees at spawn | Degrees are the author-facing unit; shaders speak radians; conversion once at spawn avoids per-frame multiply |
| Turbulence applied as positional displacement, not velocity | Velocity perturbation accumulates indefinitely and drifts; positional displacement produces oscillation that stays centred on the nominal trajectory |
| `turb_phase` randomised per particle | Without per-particle phase, all particles oscillate in sync, which looks mechanical rather than organic |
| Legacy `color_start`/`color_end` converted to gradient at YAML load time | Keeps the simulation path simple (always samples gradient), while not breaking existing data files |
| `VertexParticle::rotation` fits in existing padding | The struct was already 48 bytes (40 bytes data + 8 bytes padding for `alignas(16)` of `Color`). Using 4 bytes of padding costs nothing in memory bandwidth |

---

## Files changed

| File | Type |
|---|---|
| `src/particles/ColorStop.h` | New |
| `src/particles/ParticleSubSystemDesc.h` | Modified |
| `src/particles/ParticleEmitter.h` | Modified |
| `src/particles/ParticleEmitter.cpp` | Modified |
| `src/particles/ParticleSystemTemplate.cpp` | Modified |
| `src/editor/ParticleEditorWindow.cpp` | Modified |
| `src/core/VertexParticle.h` | Modified |
| `src/gldevices/GLVertexBuffer.cpp` | Modified |
| `data/shaders/glsl/forward/particle_forward_vs.glsl` | Modified |
| `data/shaders/glsl/geometry/particle_gbuffer_vs.glsl` | Modified |
| `data/particles/firecamp01.particles.yaml` | Modified |

---

## Things to keep in mind for future features

- `kMaxColorStops = 4` is a named constant in `ParticleSubSystemDesc`. If a more complex gradient is ever needed (e.g. 8 stops for a sunset sky), only this constant and the editor UI loop need changing.
- Turbulence is currently world-space XZ. If an emitter direction is not vertical (e.g. a horizontal jet), the perturbation axis should follow the emission direction's perpendicular plane. Track as a separate issue if needed.
- Per-particle rotation is in the billboard plane (screen-facing). True 3-D sprite rotation (e.g. sparks tumbling in 3-D) would require passing a full quaternion or 3×3 matrix, a much larger VBO change.
- The `drag` parameter interacts with turbulence: a high drag value will dampen the turbulence displacement indirectly because the particle's trajectory slows. This is physically plausible for heavy smoke but might need an independent turbulence-damping parameter for extreme values.

---

## Skills and instructions used

- `impl-issue` skill
- `src/CLAUDE.md` — Google C++ style guide, one class per header, history file requirement
- `src/particles/CLAUDE.md` — dependency graph, module structure
- `src/editor/CLAUDE.md` — panel-only UI, no logic in `Render*()` methods
- `data/shaders/glsl/CLAUDE.md` — Blend GLSL style, `#include` for uniforms
