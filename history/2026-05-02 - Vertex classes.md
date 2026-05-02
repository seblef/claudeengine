# Vertex classes (Issue #10)

## Changes

### `src/core/VertexBase.h` (new)

Plain data class for the base vertex layout: position (Vec3f), color (Color), UV (Vec2f).

| Member | Description |
|--------|-------------|
| `Vec3f position` | World-space position |
| `Color color` | RGBA vertex color |
| `Vec2f uv` | Texture coordinates |
| `VertexBase(Vec3f, Color, Vec2f)` | Attribute constructor |

### `src/core/Vertex2D.h` (new)

Inherits all members and constructors from `VertexBase`. Semantically distinct so render pipelines can dispatch on `VertexType::k2D` rather than checking layout manually.

### `src/core/Vertex3D.h` (new)

Standalone vertex for lit 3D geometry. Includes a full tangent-space basis for normal mapping.

| Member | Description |
|--------|-------------|
| `Vec3f position` | World-space position |
| `Vec3f normal` | Surface normal |
| `Vec3f binormal` | Tangent-space binormal |
| `Vec3f tangent` | Tangent-space tangent |
| `Vec2f uv` | Texture coordinates |
| `Vertex3D(Vec3f, Vec3f, Vec3f, Vec3f, Vec2f)` | Attribute constructor |

No color — lighting is evaluated per-pixel in shaders.

### `src/core/VertexType.h` (new)

`VertexType` enum and `kVertexSize` lookup table.

```cpp
enum class VertexType : uint8_t { kBase = 0, k2D, k3D, kCount };
constexpr size_t kVertexSize[];  // indexed by VertexType
```

A `static_assert` keeps the table length in sync with `kCount`.

---

## Decisions and rationale

### `Vertex2D` inherits from `VertexBase`

Both have identical GPU layouts. Inheritance eliminates constructor duplication while keeping them distinct types for enum dispatch. `using VertexBase::VertexBase` brings all constructors in without boilerplate.

### `Vertex3D` is standalone (not derived from `VertexBase`)

`Vertex3D` has no color attribute; inheriting from `VertexBase` would add an unwanted member and corrupt the GPU vertex buffer stride. The two classes share only `position` and `uv` — not enough to justify inheritance.

### No SIMD, no `alignas`

Vertices are mapped directly to GPU buffers as packed byte arrays. Compiler-inserted alignment padding would make the stride unpredictable and mismatched with GPU expectations. All members are plain floats.

### `VertexType` and `kVertexSize` in one header

Neither is a class, so they are not subject to the one-class-per-header rule. Grouping them avoids an extra include for callers that only need the lookup table.

## Output to keep in mind

- `Vertex2D` is layout-identical to `VertexBase` (`sizeof` is the same).
- `kVertexSize[static_cast<size_t>(VertexType::k3D)]` gives the stride for a `Vertex3D` buffer.
- `Vertex3D` has no color — shading is computed in the fragment shader.
- Adding a new vertex type requires: a new class header, a new `VertexType` enumerator, and a new entry in `kVertexSize` (the `static_assert` will catch a forgotten entry).

## Skills and guidance followed

- `src/CLAUDE.md`: one class per header, `#include` paths project-relative from `src/`
- Root `CLAUDE.md`: branch/PR workflow, history file, conventional commits
