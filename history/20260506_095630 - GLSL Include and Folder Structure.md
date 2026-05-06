# GLSL #include Support & Shader Folder Reorganization

**Date:** 2026-05-06
**Branch:** feat/glsl-include-and-folder-structure
**Issue:** #62

## Summary

Added a custom `#include` preprocessor to the OpenGL shader loader, then reorganized the `data/shaders/glsl/` directory into `layouts/` and `uniforms/` subfolders to eliminate duplication of vertex layout and uniform block declarations across shaders.

## Changes

### C++: `src/gldevices/GLShader.cpp`

Added two helpers inside the anonymous namespace:

- **`ParseIncludePath`** — scans a single source line for `#include <path>` or `#include "path"`, extracts and returns the inner path string. Uses manual character-by-character parsing (no regex) for simplicity and compile-time cost.
- **`ResolveIncludes`** — processes a shader source line by line. When an include directive is found, it loads the referenced file relative to `data/shaders/glsl/` and recursively expands it. An `std::unordered_set<std::string>` tracks already-included paths to prevent double-inclusion and circular includes.

The constructor now calls `ResolveIncludes` on both vertex and fragment sources before passing them to `CompileStage`.

### New GLSL layout files

| File | Purpose |
|------|---------|
| `data/shaders/glsl/layouts/base_vertex.glsl` | `VertexBase` attributes: position (loc 0), color (loc 1), uv (loc 2) |
| `data/shaders/glsl/layouts/vertex_2d.glsl` | `Vertex2D` layout — wraps `base_vertex.glsl` via `#include` |
| `data/shaders/glsl/layouts/vertex_3d.glsl` | `Vertex3D` attributes: position, normal, binormal, tangent, uv (locs 0–4) |

### New GLSL uniform file

| File | Purpose |
|------|---------|
| `data/shaders/glsl/uniforms/scene_infos.glsl` | `SceneInfosBlock` UBO definition (352 bytes, std140 row-major, binding 1) with full offset documentation |

### Updated shaders

- **`passthrough_color_2d_vs.glsl`** — replaced inline `VertexBase` layout attributes with `#include <layouts/vertex_2d.glsl>`
- **`textured_mesh_vs.glsl`** — replaced inline `Vertex3D` attributes with `#include <layouts/vertex_3d.glsl>` and the inline `SceneInfosBlock` definition with `#include <uniforms/scene_infos.glsl>`

## Decisions & Rationale

### Manual parsing over `std::regex`

Chose character-by-character parsing in `ParseIncludePath` to avoid a `<regex>` dependency and keep the per-shader startup cost minimal. The directive format is simple enough that regex adds no clarity.

### Include paths relative to shader root

All include paths are resolved relative to `data/shaders/glsl/`, not relative to the file doing the including. This keeps include paths portable and predictable regardless of how deeply nested the includer is.

### Double-inclusion guard via path set

The `included` set uses the raw path string as the key (not a canonical filesystem path). Since all paths are resolved from the same root this is sufficient and avoids calling `std::filesystem::canonical` on every include.

### `vertex_2d.glsl` includes `base_vertex.glsl`

`Vertex2D` inherits from `VertexBase` with no additional members — the GPU layout is identical. Rather than duplicating the three attribute declarations, `vertex_2d.glsl` simply re-exports `base_vertex.glsl` via `#include`. This demonstrates the recursive include capability and keeps the two types in sync without copy-paste.

## Notes for Future Contributors

- The `data/shaders/glsl/CLAUDE.md` should be updated to document the `layouts/` and `uniforms/` subfolders and the `#include` syntax.
- Include files must not contain `#version` — that directive must remain the first line of the top-level shader file.
- The `included` guard provides `#pragma once` semantics: each path is expanded at most once per shader stage compilation.
- If a DirectX 12 backend is added later, a similar `#include` preprocessor can be applied to HLSL files using the same pattern.

## Skills & Instructions Used

- `impl-issue` skill (CLAUDE.md workflow: branch from dev, implement, cpplint, commit, PR)
- Root `CLAUDE.md`: conventional commit messages, history file requirement, cpplint step
- `src/CLAUDE.md`: one-class-per-file, anonymous namespace for helpers, Google C++ style
- `data/shaders/glsl/CLAUDE.md`: shader naming conventions, Blender GLSL style
