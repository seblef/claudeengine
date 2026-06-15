# Rename light_wireframe shader to wireframe

**Issue:** #523  
**PR:** #531  
**Branch:** `feat/rename-wireframe-shader-523`  
**Date:** 2026-06-15

## Changes

- Renamed `data/shaders/glsl/light_wireframe_vs.glsl` → `wireframe_vs.glsl`
- Renamed `data/shaders/glsl/light_wireframe_ps.glsl` → `wireframe_ps.glsl`
- Updated file-level comments: removed "editor light" qualifier; now reads "wireframe pass"
- Deleted the original files

Shader logic is unchanged — it remains a generic colour-passthrough line shader that applies only the VP transform.

## Decisions and rationale

The name `light_wireframe` implied editor-only / light-specific usage, but the shader has no light-specific logic. Renaming it to `wireframe` aligns it with its actual role and with the upcoming `WireframeRenderer` (issue #521) that will call `video->CreateShader("wireframe")`.

Existing callers (`LightWireframeRenderer`, `ParticleGizmoRenderer`, `PlayerStartGizmoRenderer`) still reference `"light_wireframe"` and will break at runtime, but those renderers are scheduled for deletion in issue #525, so no update to those files is required here.

## Output to keep in mind

- The `wireframe` shader name is now canonical for the `WireframeRenderer` milestone (#19).
- Issue #525 removes `LightWireframeRenderer`, `ParticleGizmoRenderer`, and `PlayerStartGizmoRenderer`; until that lands, the old shader name reference in those files is a known dead end.
- Issue #521 introduces `WireframeRenderer`, which will be the sole user of the new `wireframe` shader.

## Skills / CLAUDE.md instructions used

- `impl-issue` skill
- `data/shaders/glsl/CLAUDE.md`: shader naming convention (vertex `_vs.glsl`, pixel `_ps.glsl`, same base name)
- Root `CLAUDE.md`: Git workflow (checkout dev, pull, feature branch, conventional commit, PR to dev with closing command)
