# Particles Core Module

**Issue**: #448  
**PR**: #456  
**Branch**: `feat/particles-core-module`  
**Date**: 2026-06-10

## Changes

### New module: `src/particles/`

| File | Role |
|---|---|
| `ParticleBlendMode.h` | Enum: `kGBuffer`, `kAdditive`, `kAlphaBlend` |
| `ParticleAnimationMode.h` | Enum: `kSequential`, `kRandom` |
| `EmitterShape.h` | Enum: `kPoint`, `kSphere`, `kBox`, `kCone` |
| `ParticleSubSystemDesc.h` | Header-only POD: emitter + sprite + lifetime settings |
| `EmbeddedLightDesc.h` | Header-only POD: flickering light embedded in a particle system |
| `ParticleSystemTemplate.h/.cpp` | Ref-counted resource: loads `data/particles/*.particles.yaml` |
| `CMakeLists.txt` | Static library `particles`, depends on `core` and `renderer` |
| `CLAUDE.md` | Module documentation for future contributors |

### `src/CMakeLists.txt`

Added `add_subdirectory(particles)` between `renderer` and `game`, reflecting the dependency graph: `game → particles → renderer`.

### `data/particles/fire.particles.yaml`

Sample asset with two sub-systems (fire body + smoke) and one embedded omni light.

## Decisions

**`core::Resource<string, T>` CRTP pattern** — mirrors `MeshTemplate` exactly: `GetOrLoad` checks the static registry before constructing, `GetAll` iterates the registry. No special exclusion logic needed (unlike `MeshTemplate`'s `__proc_` prefix) because particle templates are always file-backed.

**`video` parameter reserved** — `GetOrLoad(name, video)` accepts a `VideoDevice*` matching the issue spec, but the pointer is unused today. Future issues will use it to upload sprite atlas textures to the GPU.

**YAML sub-object syntax for ranges** — `lifetime: {min: 0.8, max: 1.5}` keeps the YAML compact and consistent with `size_start`, `size_end`, `speed`. Simple scalar keys (`emission_rate`, `spread`, `gravity`) are flat at the sub-system level.

**`std::transform` for YAML sequence parsing** — cppcheck flagged raw push_back loops as `useStlAlgorithm`. Using `std::transform` with `std::back_inserter` is idiomatic and passes the pre-commit hook.

**`cppcheck-suppress unusedStructMember`** — all struct fields that cppcheck cannot see used (because the runtime consumer is in a future issue) are suppressed inline, consistent with the rest of the codebase (`Light.h`, `GameLight.h`, etc.).

## Output to keep in mind

- **Key**: template key = basename without extension (`"fire"` for `fire.particles.yaml`).
- **Data path**: `core::Config::GetDataFolder() / "particles" / (name + ".particles.yaml")`.
- **`GetAll()`** returns every registered template; no filtering needed.
- **`EmbeddedLightDesc::phase`** is a per-instance random offset — it is zero in the descriptor and should be randomised by the runtime when spawning a particle system instance (not stored in the YAML).
- **Sprite animation**: `animation_fps == 0` means pick a random frame per particle (not animated). The `sprite_cols`/`sprite_rows` fields define the sheet layout.
- **`lit` flag**: only meaningful when `blend_mode == kAlphaBlend`; ignored for `kGBuffer` and `kAdditive`.

## Skills / instructions used

- `impl-issue` skill
- `src/CLAUDE.md` — one class per file, include root `src/`, Google C++ style
- `src/game/CLAUDE.md` — `MeshTemplate` as the reference pattern for `GetOrLoad` / `GetAll`
- Project `CLAUDE.md` — conventional commits, history file, PR to `dev`
