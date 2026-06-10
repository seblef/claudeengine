# CLAUDE.md — particles module

## Role

The `particles` module owns all CPU simulation, GPU upload, and rendering
logic for particle systems. It sits below `renderer` in the dependency graph
(same role as `terrain/`).

## Dependency graph

```
game → renderer → particles → abstract → core
```

`ParticleRenderer` lives in `particles` and is consumed by `renderer`.
`particles` must not be included by `abstract` or `core`.

## Module structure

| File(s) | Responsibility |
|---|---|
| `ParticleBlendMode.h` | Enum: kGBuffer, kAdditive, kAlphaBlend |
| `ParticleAnimationMode.h` | Enum: kSequential, kRandom |
| `EmitterShape.h` | Enum: kPoint, kSphere, kBox, kCone |
| `ParticleSubSystemDesc.h` | Header-only POD: per-sub-system emitter + visual settings |
| `EmbeddedLightDesc.h` | Header-only POD: dynamic light embedded in a particle system |
| `ParticleSystemTemplate` | Ref-counted resource loading `*.particles.yaml` from `data/particles/` |
| `ParticleEmitter` | CPU simulation + GPU VBO upload for one sub-system |
| `ParticleRenderer` | Geometry-pass rendering of kGBuffer emitters into the G-buffer |

## Data files

Particle definitions live in `data/particles/*.particles.yaml`.
The template key is the basename without extension (e.g. `"fire"` for
`fire.particles.yaml`).

## Guidelines

Follow all rules in `src/CLAUDE.md`. Additionally:
- One class/struct per `.h` file.
- Structs used purely as data holders may be header-only.
- Do not include platform or OpenGL headers directly; go through `abstract/`.
