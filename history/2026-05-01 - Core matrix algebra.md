# Core matrix algebra

**Date:** 2026-05-01
**Branch:** feat/core-matrix-algebra
**Issue:** #2

---

## Summary

Added `Mat2f`, `Mat3f`, and `Mat4f` — 2×2, 3×3, and 4×4 float matrices — to the `core` module, completing the linear algebra foundation needed for 3D transforms, rendering, and physics. Also introduced Google Test as the project's unit testing framework and wired it into CMake.

---

## New files

| File | Role |
|------|------|
| `src/core/Mat2f.h` | 2×2 matrix, scalar-only, header-only |
| `src/core/Mat3f.h` | 3×3 matrix, SSE4.2 SIMD on multiply/add/subtract/transpose, header-only |
| `src/core/Mat4f.h` | 4×4 matrix, SSE4.2 SIMD on multiply/transpose, header-only |
| `tests/CMakeLists.txt` | Test subdirectory wiring |
| `tests/core/CMakeLists.txt` | `core_tests` binary: links GTest + core |
| `tests/core/Mat2fTest.cpp` | 13 tests for Mat2f |
| `tests/core/Mat3fTest.cpp` | 22 tests for Mat3f |
| `tests/core/Mat4fTest.cpp` | 20 tests for Mat4f |

## Modified files

| File | Change |
|------|--------|
| `CMakeLists.txt` | Added GTest v1.14.0 via FetchContent, `enable_testing()`, `add_subdirectory(tests)` |

---

## Design decisions

### Storage layout — row-major with aligned rows

All matrices use `alignas(16) float data_[N]` in row-major order. Strides:

- **Mat2f**: stride 2, `data_[row*2+col]`, no alignment (4 elements — SSE overhead exceeds benefit)
- **Mat3f**: stride 4 (4 floats per row, `data_[row*4+3]=0` padding), `alignas(16)` — mirrors Vec3f's w=0 convention
- **Mat4f**: stride 4, `alignas(16)` — all 4 rows at offsets 0/16/32/48 bytes, all 16-byte aligned

Using flat `float data_[]` storage (rather than `__m128 reg_[]`) keeps `operator()(row, col)` a simple index with no temporaries, and lets SIMD code load/compute/store as needed.

### SIMD strategy

Mat3f and Mat4f use the "row as linear combination of B rows" multiply:

```
result_row_i = broadcast(A[i][0]) * B_row_0
             + broadcast(A[i][1]) * B_row_1
             + broadcast(A[i][2]) * B_row_2   // Mat3f stops here
             + broadcast(A[i][3]) * B_row_3   // Mat4f only
```

Broadcast is done with `_mm_shuffle_ps(a, a, _MM_SHUFFLE(k,k,k,k))`. The Mat3f padding lane (index 3) stays 0 through all operations since 0×anything=0.

Transpose uses `_MM_TRANSPOSE4_PS` (standard SSE macro from `<immintrin.h>`). For Mat3f, a zero register is passed as the fourth row and discarded after the macro.

Inverse uses scalar cofactor expansion for all three types — the readability cost of SIMD cofactors is not worth it at the frequency matrices are inverted in game code.

### Mat4f inverse — Cramer's rule (Lengyel method)

Six 2×2 sub-determinants from rows 0–1 (`p0..p5`) and six from rows 2–3 (`q0..q5`) are precomputed, then combined into the 16 adjugate entries in 48 multiply-add operations. This is the standard reference approach from "Foundations of Game Engine Development Vol. 1" (Lengyel, 2016). Verified algebraically against the cofactor expansion definition.

### Euler angle convention — extrinsic ZYX

`Mat3f::Rotation(euler)` and `Mat4f::Rotation(euler)` produce `Rz(z) * Ry(y) * Rx(x)`. This is the most common game-engine convention (extrinsic, global axes). Applied to a column vector: X rotation first, then Y, then Z. **Future Quaternion work must match this convention.**

### Mat4f::operator=(const Mat3f&)

Copies the upper-left 3×3, sets column 3 of rows 0–2 to 0, and row 3 to `[0,0,0,1]`. This is the natural embedding of a rotation/scale matrix into a homogeneous 4×4. `Mat4f.h` includes `Mat3f.h` — an intentional cross-class dependency within the same `core` module (not a circular include).

### Mat2f::RotationScale2D ordering

`RotationScale2D(angle, scale) = Scale2D(scale) * Rotation2D(angle)`. This means a column vector is first rotated then scaled (non-uniformly if sx ≠ sy). Documented in the header.

### Google Test v1.14.0

Fetched via `FetchContent` with a pinned tag (reproducible builds). `BUILD_GMOCK OFF` reduces compile time. Tests are registered with `gtest_discover_tests` so each `TEST()` case is an individual CTest test, enabling filtering with `ctest -R Mat4f`.

**Run tests:** `cmake --build _build && ctest --test-dir _build`

---

## Test results

```
55 tests from 3 test suites.  100% passed.
Mat2fTest:  13 tests
Mat3fTest:  22 tests
Mat4fTest:  20 tests
```

Key coverage: identity, element get/set, multiply (known values + identity), add/subtract (Mat3f), inverse roundtrip (`M * M⁻¹ = I`), transpose roundtrip, all factory methods with known expected values.

---

## Skills and CLAUDE.md instructions used

**Skills:** none beyond native Claude Code capabilities.

**CLAUDE.md instructions followed:**
- One class per header file; header-only for matrix types (no `.cpp`)
- Google C++ Style Guide: trailing `_` on private members, PascalCase methods, `kName` constants, `[[nodiscard]]` on all const/query methods
- Include root is `src/` (`#include "core/Mat3f.h"`)
- SIMD guard pattern `#if CORE_SIMD_ENABLED` with scalar fallback
- `alignas(16)` for SSE-compatible types
- Git workflow: feature branch → implementation → PR

---

## Output to keep in mind for future features

- **Euler angle convention is extrinsic ZYX** — quaternion SLERP, camera look-at, and physics rotation must use this same order or explicitly convert.
- **Mat3f row stride is 4** (not 3) — any code iterating over raw `data_` must account for the padding lane.
- **Mat4f includes Mat3f.h** — this is intentional; adding other cross-core-type includes is acceptable as long as there are no circular dependencies.
- **`ctest --test-dir _build -R Mat4fTest`** filters to a specific matrix type for fast iteration.
- Quaternion (`Quat4f`) is the natural next addition to the core linear algebra module.
