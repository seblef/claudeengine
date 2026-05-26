# TerrainPatchMesh — shared patch VBO/IBO for all LOD levels

**Issue:** #285
**Branch:** feat/terrain-patch-mesh-285
**Date:** 2026-05-26

## What was implemented

### New files

| File | Purpose |
|------|---------|
| `src/core/VertexTerrain.h` | New vertex type: XZ position only (two floats) |
| `src/terrain/TerrainPatchMesh.h` | Class declaration |
| `src/terrain/TerrainPatchMesh.cpp` | Grid generation + GPU upload |
| `data/shaders/glsl/layouts/vertex_terrain.glsl` | GLSL input layout for terrain VS |

### Modified files

| File | Change |
|------|--------|
| `src/core/VertexType.h` | Added `kTerrain` enum value + `sizeof(VertexTerrain)` entry |
| `src/terrain/CMakeLists.txt` | Added `TerrainPatchMesh.cpp` |
| `src/terrain/CLAUDE.md` | Documented new class |

## Decisions and rationale

### Vertex layout: `Vec2f` (XZ only)
The heightmap Y and UV coordinates are derived at runtime in the vertex shader from the XZ position plus per-patch uniforms (origin, scale, morph). Storing only XZ halves the per-vertex bandwidth compared to `Vec3f` and avoids redundant data.

### New `VertexType::kTerrain`
The existing `VertexType` enum was extended with `kTerrain` so the generic buffer infrastructure can handle terrain VBOs transparently (stride, binding, etc.).

### Index type: `uint16_t`
For `patch_size = 64`, `(64+1)² = 4225` vertices — well within the 65535 uint16 limit. The class asserts `patch_size ≤ 255` (which gives exactly 65536 vertices, the upper bound). Using uint16 over uint32 saves 50 % of IBO memory and typically improves GPU cache behaviour.

### Winding order
Triangles are emitted CCW (when viewed from +Y): `(v00, v10, v11)` then `(v00, v11, v01)`. This matches OpenGL default front-face convention for a Y-up terrain viewed from above.

### `BufferUsage::kImmutable`
The geometry never changes after upload, so `kImmutable` allows the driver to place the buffers in the fastest available GPU memory.

## Output to keep in mind for next features

- **Terrain shader**: the terrain vertex shader must `#include "layouts/vertex_terrain.glsl"` and derive Y from a heightmap texture + the patch origin/scale uniforms.
- **Per-patch uniforms**: each draw call needs a constant buffer containing `patch_origin` (world XZ), `patch_scale` (world units per local unit), and `morph_factor` (CDLOD smooth LOD transition) — these are intentionally absent from `TerrainPatchMesh`.
- **Instanced rendering**: a future optimisation could replace per-draw-call constant buffers with a structured buffer of patch descriptors and one instanced draw call.
- **Patch size constraint**: `patch_size ≤ 255` is enforced by assert. If larger patches are ever needed, change `IndexType` to `kUInt32`.

## Skills / instructions followed

- `src/CLAUDE.md`: one class per `.h/.cpp`, Google C++ style, include root is `src/`.
- `src/terrain/CLAUDE.md`: only `core` and `abstract` dependencies; Y-up coordinate system.
- `data/shaders/glsl/CLAUDE.md` + `.claude/rules/glsl-shaders.md`: `#include` for layouts; separate VS/PS files; Blend GLSL coding style.
- Root `CLAUDE.md`: conventional commit message, history file, PR to `dev`.
