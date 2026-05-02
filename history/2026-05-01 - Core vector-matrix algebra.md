# Core vector/matrix algebra operators (Issue #4)

## Changes

### `src/core/Vec2f.h`
- Forward-declares `class Mat2f`
- Declares `Vec2f operator*(const Mat2f&) const` and `Vec2f& operator*=(const Mat2f&)` as member functions (bodies in Mat2f.h)

### `src/core/Vec3f.h`
- Forward-declares `class Mat3f` and `class Mat4f`
- Declares `Vec3f operator*(const Mat3f&) const`, `Vec3f& operator*=(const Mat3f&)`, `Vec3f operator*(const Mat4f&) const`, `Vec3f& operator*=(const Mat4f&)` as member functions

### `src/core/Vec4f.h`
- Forward-declares `class Mat4f`
- Declares `Vec4f operator*(const Mat4f&) const` and `Vec4f& operator*=(const Mat4f&)` as member functions

### `src/core/Mat2f.h`
- Added `Data() const → const float*` accessor for SIMD pointer arithmetic
- Out-of-line definition of `Vec2f::operator*(const Mat2f&)` (scalar)
- Out-of-line definition of `Vec2f::operator*=(const Mat2f&)`

### `src/core/Mat3f.h`
- Added `Data() const → const float*` accessor
- Out-of-line definition of `Vec3f::operator*(const Mat3f&)` — SIMD: `_mm_dp_ps` mask `0x71` (xyz lanes) per row
- Out-of-line definition of `Vec3f::operator*=(const Mat3f&)`

### `src/core/Mat4f.h`
- Added `#include "core/Vec4f.h"`
- Added `Data() const → const float*` accessor
- Out-of-line definition of `Vec3f::operator*(const Mat4f&)` — treats `this` as homogeneous point (implicit w=1); SIMD builds `[x, y, z, 1]` with `_mm_set_ps` and uses mask `0xF1`
- Out-of-line definition of `Vec3f::operator*=(const Mat4f&)`
- Free function `TransformNoTranslation(const Vec3f&, const Mat4f&)` — applies upper-left 3×3 only; SIMD uses `Reg()` (w_=0) with mask `0x71`
- Out-of-line definition of `Vec4f::operator*(const Mat4f&)` — full 4×4 dot per row, mask `0xF1`
- Out-of-line definition of `Vec4f::operator*=(const Mat4f&)`

### Test files
- `tests/core/Mat2fTest.cpp`: 4 new tests (identity no-op, quarter-turn rotation, scale, `*=` consistency)
- `tests/core/Mat3fTest.cpp`: 4 new tests
- `tests/core/Mat4fTest.cpp`: 9 new tests including point-vs-direction distinction for `Vec4f` and `TransformNoTranslation`

**Total: 73 tests, all passing.**

## Decisions and rationale

### Methods on Vec, not free functions

The operators are declared as member functions on the Vec classes. This gives cleaner call syntax (`v *= m` with unambiguous lookup) and makes the API browsable on the type. The circular dependency (Vec cannot include Mat) is resolved by:

1. **Forward declaration** of the matrix type in the Vec header
2. **Out-of-line definition** of the method body in the Mat header (where the matrix type is complete)

Any TU that needs `v * M` must include the Mat header anyway (to have a `Mat` object), which is exactly when the definition becomes available.

### `v * M` semantics (column-vector convention)

The matrix stores translation in the last **column** (standard OpenGL/math convention). A pure row-vector multiply would not apply translation, making `v * Translation(t)` a no-op.

Instead, `v * M` computes **M applied to v** (column-vector semantics):
```
result[k] = Σ_j M(k,j) * v_j   (with implicit v.w = 1 for Vec3f × Mat4f)
```
Written with v on the left for ergonomic syntax (`v *= modelMatrix`). Mathematically: `result[k] = dot(M_row_k, v)`.

### `Vec3f * Mat4f` vs `TransformNoTranslation`

- `Vec3f * Mat4f`: implicit w=1 — applies full transform including translation. Use for **points**.
- `TransformNoTranslation(v, M)`: implicit w=0 — applies only the upper-left 3×3. Use for **directions and normals**.

`TransformNoTranslation` stays a free function (two independent inputs, no owning type).

### SIMD details

- `Vec3f * Mat3f`: `_mm_dp_ps(row, Reg(), 0x71)` — xyz-only dot (w_=0 padding excluded)
- `Vec3f * Mat4f`: builds `[x, y, z, 1]` via `_mm_set_ps(1.f, z, y, x)` (reversed arg order), mask `0xF1` (all 4 lanes)
- `TransformNoTranslation`: reuses `Reg()` directly — w_=0 already zeroes the translation column
- `Vec4f * Mat4f`: `_mm_dp_ps(row, Reg(), 0xF1)` per row

## Output to keep in mind

- `v * M` means **M applied to v** (column-vector). Future code should follow this convention.
- `TransformNoTranslation` is the correct path for lighting normals and non-homogeneous vectors.
- `Data()` is `const`-only — callers cannot mutate matrix internals through raw pointers.
- Future Quaternion work: `v * q` should follow the same pattern (declare in Vec3f.h, define in Quat.h).

## Skills and guidance followed

- `src/CLAUDE.md`: one class per header, `#include` paths relative to `src/`, Google C++ style guide
- Root `CLAUDE.md`: branch/PR workflow, history file required, conventional commits
