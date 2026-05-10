# Material Color Properties (ambient, diffuse, emissive, shininess)

**Issue:** #80  
**Branch:** feat/material-color-properties  
**Date:** 2026-05-10

## Summary

Extended the `Material` class with per-material Blinn-Phong color properties and wired them into a new UBO slot (slot 3) uploaded before each draw call.

## Changes

### New `src/renderer/MaterialInfos.h`
64-byte std140 struct (4 float4s) for UBO slot 3:
- `diffuse_color` (Vec3f, 16 B) — diffuse texture tint, default (1,1,1)
- `emissive_color` (Vec3f, 16 B) — emissive texture scale, default (0,0,0)
- `ambient_color` (Vec3f, 16 B) — additive ambient term, default (0,0,0)
- `shininess` (float, 4 B) — Blinn-Phong exponent, default 32
- 12 bytes implicit tail padding (struct aligned to 16 due to Vec3f)

Vec3f is `alignas(16)` with `w_=0`, so no explicit padding members are needed — the 4th float of each Vec3f slot fills the std140 implicit vec3 padding exactly.

### Modified `src/renderer/MaterialDesc.h`
Added builder setters/getters: `SetDiffuseColor`, `SetEmissiveColor`, `SetAmbientColor`, `SetShininess` with defaults matching `Material`.

### Modified `src/renderer/Material.h/.cpp`
- Added `diffuse_color_`, `emissive_color_`, `ambient_color_`, `shininess_` fields
- `Material(const MaterialDesc&, ...)` constructor now reads color properties from desc
- `LoadFromYaml()` now parses optional `colors:` YAML node (diffuse, emissive, ambient, shininess)
- `UploadColors(ConstantBuffer*)` fills and uploads `MaterialInfos`
- `IsEmissive()` returns true if `emissive_color` or `ambient_color` is non-zero

### New `data/shaders/glsl/uniforms/material_infos.glsl`
GLSL UBO block at `binding = 3` matching `MaterialInfos` layout.

### Modified `src/renderer/Renderer.h/.cpp`
- Adds `material_infos_cb_` (slot 3) created alongside the existing CBs
- `Update()` binds all three CBs
- `SetMaterialInfos(const Material&)` calls `material.UploadColors(cb)` — mirrors the pattern of `SetRenderableInfos`

### Modified `src/renderer/MeshRenderer.cpp`
Calls `Renderer::Instance().SetMaterialInfos(*mat)` whenever the active material changes (alongside `mat->Set()` for texture binding).

## Key Design Decisions

**No explicit Vec3f padding:** Unlike the original plan which showed `float pad0_` after each Vec3f, Vec3f is already 16 bytes (SIMD-aligned) so the struct reaches 64 bytes naturally with only `shininess` + implicit tail padding. Added `offsetof` static_asserts to catch any future misalignment.

**IsEmissive() checks colors only:** A non-default emissive texture with `emissive_color=(0,0,0)` and `ambient_color=(0,0,0)` would output black, so only colors are checked. This avoids the complexity of comparing texture pointers.

**SetMaterialInfos on Renderer:** Follows the same dispatch pattern as `SetRenderableInfos` — MeshRenderer doesn't hold a direct pointer to the CB; it goes through `Renderer::Instance()`.

## YAML Format
```yaml
colors:
  diffuse:  [1.0, 0.8, 0.6]
  emissive: [0.2, 0.1, 0.0]
  ambient:  [0.05, 0.05, 0.05]
  shininess: 64.0
```

## Next

Issue #81 — G-buffer shaders (gbuffer_vs/ps, emissive_ps) — depends on this issue (MaterialInfos UBO layout).

## Skills and Guidelines Used

- `src/CLAUDE.md` — one class per file, Google C++ style, include root = `src/`
- `impl-issue` skill — git workflow, cpplint, conventional commit, PR to dev
