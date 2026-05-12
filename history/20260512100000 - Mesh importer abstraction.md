# Mesh Importer Abstraction

**Issue**: #112 — Mesh Pipeline v1 milestone
**Branch**: feat/mesh-importer-abstraction

## Changes

Two new files added to `src/mesh/`:

| File | Role |
|---|---|
| `IMeshImporter.h` | Pure abstract interface all importers must implement |
| `MeshUtils.h` | Declarations for shared geometry processing utilities |
| `MeshUtils.cpp` | Implementations: `ComputeNormals`, `ComputeTangents`, `WeldVertices` |

`src/mesh/CMakeLists.txt` updated to compile `MeshUtils.cpp`.

## Interface Design

```cpp
class IMeshImporter {
 public:
  virtual ~IMeshImporter() = default;
  virtual bool Import(const std::string& path, MeshData* mesh) const = 0;
};
```

Single method returning bool; errors are logged inside the implementation.
The `const` qualifier on `Import` marks importers as stateless functors — no
member state carries between calls.

## Utility Algorithms

### ComputeNormals
Area-weighted average of incident face normals:
1. Accumulate the raw cross-product (not normalised) for each triangle into the three vertex normal accumulators — longer edges contribute proportionally more weight.
2. Normalise each accumulator; degenerate vertices fall back to `+Y`.

### ComputeTangents (Lengyel's method)
1. Per triangle: solve the 2×2 UV-edge system to get face tangent `T` and bitangent `BT`.
2. Accumulate per vertex.
3. Gram-Schmidt orthogonalise `T` against `N`; compute handedness via `(N × T) · BT` to set the sign of the stored binormal.
   Degenerate UV triangles (zero UV area) are skipped; degenerate vertices fall back to `(+X, +Z)`.

### WeldVertices
Hash-map approach (O(n) amortised):
- Key: `(position.xyz, normal.xyz, uv.xy)` — 8 floats, exact equality.
- Tangent and binormal are excluded from the key; welding is designed to run before `ComputeTangents`.
- FNV-1a over raw key bytes — fast, good distribution for float data.
- Rebuilds both `vertices` and `indices` vectors in-place.

## Decisions

- **Exact float equality for welding** — importers provide consistent float values for truly duplicate vertices; epsilon welding adds complexity without a clear benefit at this stage.
- **Tangent/binormal excluded from weld key** — welding precedes tangent computation in the standard pipeline; including computed tangents would prevent useful merges.
- **Free functions in MeshUtils, not a class** — matches the project's `*Utils.cpp` convention; no state is required.
- **Vec3f addition in ComputeNormals** — the SIMD `w_=0` invariant is preserved throughout accumulation since every operand has `w_=0`.

## Next steps

- **Issue #113**: `ObjImporter : IMeshImporter` using `fast_obj`.
- **Issue #114**: `FbxImporter : IMeshImporter` using `ufbx`.
- Both importers call `WeldVertices` then `ComputeNormals`/`ComputeTangents` as needed based on what the source format provides.

## Skills and guidelines used

- `impl-issue` skill workflow
- `src/CLAUDE.md`: one class per .h, utility functions in `*Utils.cpp`, Google C++ style
- `CPPLINT.cfg`: linelength=120, suppressed legal/copyright
