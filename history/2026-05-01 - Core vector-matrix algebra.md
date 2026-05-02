# Core vector/matrix algebra operators (Issue #4)

## Changes

### `src/core/Mat2f.h`
- Added `Vec2f operator*(const Vec2f&, const Mat2f&)` free function
- Added `Vec2f& operator*=(Vec2f&, const Mat2f&)` free function

### `src/core/Mat3f.h`
- Added `Data() const → const float*` accessor (needed by free functions for SIMD pointer access)
- Added `Vec3f operator*(const Vec3f&, const Mat3f&)` free function (SIMD: `_mm_dp_ps` with mask `0x71` per row)
- Added `Vec3f& operator*=(Vec3f&, const Mat3f&)` free function

### `src/core/Mat4f.h`
- Added `#include "core/Vec4f.h"`
- Added `Data() const → const float*` accessor
- Added `Vec3f operator*(const Vec3f&, const Mat4f&)` — treats v as a homogeneous point (implicit w=1); SIMD builds `[v.x, v.y, v.z, 1]` with `_mm_set_ps` and uses `_mm_dp_ps` mask `0xF1`
- Added `Vec3f& operator*=(Vec3f&, const Mat4f&)`
- Added `TransformNoTranslation(const Vec3f&, const Mat4f&)` — applies only the upper-left 3×3; SIMD uses `v.Reg()` (w_=0) with mask `0x71`
- Added `Vec4f operator*(const Vec4f&, const Mat4f&)` — full 4×4 dot per row, mask `0xF1`
- Added `Vec4f& operator*=(Vec4f&, const Mat4f&)`

### Test files
- `tests/core/Mat2fTest.cpp`: 4 new tests (identity no-op, quarter-turn rotation, scale, `*=` consistency)
- `tests/core/Mat3fTest.cpp`: 4 new tests
- `tests/core/Mat4fTest.cpp`: 9 new tests including point-vs-direction distinction for `Vec4f` and `TransformNoTranslation`

**Total: 73 tests, all passing.**

## Decisions and rationale

### `v * M` semantics (column-vector convention)

The matrix stores translation in the last **column** (standard OpenGL/math convention). A pure row-vector multiply (`(v * M)_j = Σ_i v_i M_ij`) would not apply translation, making `v * Translation(t)` a no-op — useless for a game engine.

Instead, `v * M` is defined to compute **M applied to v** (column-vector semantics):
```
result[k] = Σ_j M(k,j) * v_j   (with implicit v.w = 1 for Vec3f)
```
Written with v on the left for syntactic convenience (`v *= modelMatrix`), consistent with how game engines like DirectX Math expose row-vector APIs.

This means `result[k] = dot(M_row_k, v)` — dot product of each row of M with the (extended) vector.

### `Vec3f * Mat4f` vs `TransformNoTranslation`

- `Vec3f * Mat4f`: implicit w=1, applies full transform including translation. Use for **points**.
- `TransformNoTranslation`: implicit w=0, ignores translation. Use for **direction vectors and normals**.

The SIMD path for `Vec3f * Mat4f` builds `[v.x, v.y, v.z, 1.f]` via `_mm_set_ps(1.f, v.z, v.y, v.x)` (note reversed argument order — `_mm_set_ps` takes elements from high to low). Mask `0xF1` includes all 4 lanes.

`TransformNoTranslation` reuses `v.Reg()` directly since `w_=0` zeroes the translation column contribution. Mask `0x71` makes the intent explicit.

### Free functions live in Mat headers, not Vec headers

Circular dependency prevention: Vec headers cannot include Mat headers. Free functions in Mat headers work because Mat already includes Vec. `Data()` was added to expose `data_` for SIMD pointer arithmetic without making the array public.

## Output to keep in mind

- `v * M` throughout the codebase means **M applied to v** (column-vector). Document this clearly in any public-facing API.
- `TransformNoTranslation` is the correct function for lighting normals, direction vectors, and any non-homogeneous transforms.
- Future Quaternion work should maintain the same convention: `v * q` applies quaternion rotation to v.
- `Data()` accessor is intentionally `const`-only — callers should not mutate matrix internals through raw pointers.

## Skills and guidance followed

- `src/CLAUDE.md`: one class per header, `#include` paths relative to `src/`, Google C++ style guide
- Root `CLAUDE.md`: branch/PR workflow, history file required, conventional commits
- Plan file: SIMD `_mm_dp_ps` mask conventions, free-function placement in Mat headers
