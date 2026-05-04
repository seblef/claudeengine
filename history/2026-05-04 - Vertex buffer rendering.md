# Vertex Buffer Rendering

**Date:** 2026-05-04
**Branch:** feat/vertex-buffer-rendering
**Issue:** #33

## Summary

Added support for rendering from vertex buffers. The engine can now draw geometry to the screen. The demo quad in `main.cpp` is now visible.

## Changes

### New file: `src/abstract/PrimitiveType.h`
Enum class `abstract::PrimitiveType` with six topologies:
- `kPointList`, `kLineList`, `kLineStrip`, `kTriangleList`, `kTriangleStrip`, `kTriangleFan`

### `src/abstract/VideoDevice.h`
Two new pure-virtual methods:
- `SetPrimitiveType(PrimitiveType)` — stores the topology for the next draw call
- `Render(int num_vertices, int first_vertex = 0)` — issues the draw call

### `src/gldevices/GLVideoDevice.h` / `.cpp`
- `SetPrimitiveType`: switch mapping `PrimitiveType` → GL enum stored as `GLenum primitive_type_`
- `Render`: calls `glDrawArrays(primitive_type_, first_vertex, num_vertices)`
- Added `<GL/gl.h>` include to the header (for `GLenum` type in member declaration)
- Added `#define GL_GLEXT_PROTOTYPES` to `.cpp` (for `glDrawArrays` prototype)

### `src/gldevices/GLVertexBuffer.h` / `.cpp`
Added a `GLuint vao_` member. Each `GLVertexBuffer` now owns a VAO pre-configured with vertex attribute pointers for its layout:

- `kBase` / `k2D`: location 0=position(vec3), 1=color(vec4), 2=uv(vec2)
- `k3D`: location 0=position(vec3), 1=normal(vec3), 2=binormal(vec3), 3=tangent(vec3), 4=uv(vec2)

`Bind()` now binds the VAO (which restores both the VBO binding and all attribute pointers in a single call). `Fill()` is unchanged — it binds the VBO directly for data upload, which is correct and doesn't disturb the VAO state.

### `src/app/main.cpp`
Added four lines inside the render loop:
```cpp
shader->Activate();
vb->Bind();
video->SetPrimitiveType(abstract::PrimitiveType::kTriangleList);
video->Render(6);
```

## Design Decisions

### VAO ownership: per-buffer vs. per-shader vs. global
OpenGL 4.6 core profile requires a bound VAO for `glDrawArrays`. Three options were considered:

1. **Per-buffer VAO** (chosen): Each `GLVertexBuffer` creates and owns a VAO on construction, pre-configured for its vertex layout. `Bind()` binds the VAO. Pro: encapsulated, no extra state needed in `GLVideoDevice`, `Render()` is a single `glDrawArrays` call.

2. **Per-device VAO**: A single global VAO in `GLVideoDevice`, reconfigured on each `Bind()`. Pro: fewer VAOs. Con: requires `GLVideoDevice` to know the vertex type, breaking the clean separation.

3. **Per-shader/pipeline VAO**: VAO shared between shader and buffer. Pro: matches modern design. Con: premature abstraction for this stage of the engine.

Option 1 was chosen as cleanest at this stage of the engine.

### `offsetof` warnings on non-standard-layout types
`VertexBase` and `Vertex3D` have user-defined constructors, making them non-standard-layout. GCC emits `-Winvalid-offsetof` warnings when `offsetof` is used on these types. This is "conditionally-supported" per C++17 §17.7 [support.types.layout], and GCC/Clang support it correctly since the structs contain only plain floats with no virtual dispatch or multiple inheritance. The `VertexBase` docstring explicitly states "no padding or alignment hints". No action taken.

### SetPrimitiveType and Render as separate methods
Following the pattern of existing OpenGL state machines — set state, then draw — matches how both the D3D12 and GL backends expect to operate. Keeping them separate allows callers to set the topology once and draw many times without repeated switches.

## Next Steps / Output to Remember

- `GLVertexBuffer::Bind()` now binds the VAO, not just the VBO. Any code that previously relied on `Bind()` setting `GL_ARRAY_BUFFER` without also needing a VAO will need to be updated (none at this stage).
- Vertex attribute locations are hard-coded per vertex type in `ConfigureAttributes()`. They must match the `layout(location=N)` declarations in GLSL shaders. Current agreement: `passthrough_color_2d` uses locations 0=position, 1=color, 2=uv. Future shaders must follow the same convention per vertex type.
- `SetPrimitiveType` is persistent state on `GLVideoDevice` (mirrors GL state machine). No need to call it every frame unless the topology changes.
- A `SetVertexBuffer(VertexBuffer*)` method on `VideoDevice` was deliberately not added — `VertexBuffer::Bind()` remains the caller's responsibility, keeping the API minimal.

## Skills and Instructions Used

- `impl-issue` skill
- `CLAUDE.md`: git workflow (feat/ prefix, conventional commits, history file, PR)
- `src/CLAUDE.md`: one class per file, include path from `src/`, Google C++ style guide
