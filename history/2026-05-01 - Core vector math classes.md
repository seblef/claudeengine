# Core vector math classes

**Date:** 2026-05-01
**Branch:** `feat/core-vector-math`
**Skill files used:** none (built-in planning + implementation)

---

## What was added

Four new files in `src/core/`:

| File | Purpose |
|------|---------|
| `Vec2f.h` | 2-component float vector |
| `Vec3f.h` | 3-component float vector (SSE-optimised) |
| `Vec4f.h` | 4-component float vector (SSE-optimised) |
| `MathUtils.h` | Math constants + utility function declarations |
| `MathUtils.cpp` | `FastInvSqrt`, `NearlyEqual`, `Clamp`, `Lerp`, `ToRadians`, `ToDegrees` |

`src/core/CMakeLists.txt` was updated to:
- Add `MathUtils.cpp` to the `core` library sources
- Detect SSE4.2 support via `CheckCXXCompilerFlag` and set `CORE_SIMD_ENABLED`

---

## Operations per class

### Vec2f / Vec3f / Vec4f (common)
- `operator+`, `-`, `* scalar`, `/ scalar`, unary `-`
- Compound assignment: `+=`, `-=`, `*=`, `/=`
- `operator==`, `!=`
- `Dot()`, `LengthSquared()`, `Length()`, `Normalized()`

### Vec3f only
- `Cross()` — 3D cross product using SIMD shuffle trick

### Vec2f only
- `Cross()` — returns scalar z-component of 3D cross: `x*rhs.y - y*rhs.x`

### Constants
- `Vec2f`: `kZero`, `kOne`, `kAxisX`, `kAxisY`
- `Vec3f`: `kZero`, `kOne`, `kAxisX`, `kAxisY`, `kAxisZ`
- `Vec4f`: `kZero`, `kOne`, `kAxisX`, `kAxisY`, `kAxisZ`, `kAxisW`

### MathUtils
- `FastInvSqrt(float)` — SIMD `_mm_rsqrt_ss` + Newton-Raphson (scalar: Quake III `std::bit_cast`)
- `NearlyEqual(float, float, float epsilon)`
- `Clamp(float, float, float)`
- `Lerp(float, float, float)`
- `ToRadians(float)`, `ToDegrees(float)`
- Constants: `kPi`, `kHalfPi`, `kTwoPi`, `kDegToRad`, `kRadToDeg`

---

## Key decisions and rationale

### Header-only vector classes
SIMD gains require inlining — moving implementations to `.cpp` would force the compiler to emit function calls. All methods are `inline` and defined in the header. Constants are declared in the class body and defined after the class (with `inline`) because the class type is incomplete at the point of the in-class declaration.

### `__m128` storage for Vec4f, `float[4]` for Vec3f
- `Vec4f`: `__m128 reg_` stores the register directly, making the class a zero-cost wrapper when kept in XMM registers at `-O2`.
- `Vec3f`: `alignas(16) float data_[4]` with `w=0` padding. This allows constexpr-compatible construction, direct field access, and clean `_mm_load_ps` usage. The `alignas(16)` on the class ensures the aligned load variant is always safe.
- `Vec2f`: plain `float x_, y_` — two-lane SSE wastes half the register and adds shuffle overhead; the compiler auto-vectorizes bulk float arrays better without interference.

### SSE4.1 baseline, no `-march=native`
`_mm_dp_ps` (dot product in a single instruction) requires SSE4.1. Available on all x86-64 hardware since ~2007. `-march=native` is not used because it breaks cross-compilation and can produce binaries that crash on older machines. AVX/AVX2 can be added later via a CMake option when Vec8f or matrix types are introduced.

### `CORE_SIMD_ENABLED` compile-time flag
A single `#if CORE_SIMD_ENABLED` guard separates SIMD and scalar paths. Both paths are complete and correct, allowing builds on platforms without SSE4.2. The flag is PUBLIC on the `core` target so all consumers inherit it.

### `FastInvSqrt` dual implementation
- SIMD path: `_mm_rsqrt_ss` (hardware estimate, ~12-bit) + one Newton-Raphson step → ~23-bit precision (full float mantissa). This is the modern standard.
- Scalar path: Quake III Q_rsqrt using `std::bit_cast<int32_t>` (C++20) for safe type-punning — no UB, no union trick, no `memcpy`.

### `inline static const` after-class pattern
`inline const Vec3f Vec3f::kZero{0.f, 0.f, 0.f}` after the class definition is valid C++17. It:
- Is an inline variable (no ODR violations across TUs)
- Has the class fully defined so constructors resolve
- Avoids a separate `.cpp` file just for constants

---

## Output to keep in mind

- All SIMD operations assume `CORE_SIMD_ENABLED` at compile time — no runtime dispatch.
- `Vec3f` alignment contract: stack and `std::vector` are safe; raw `new Vec3f[N]` is safe in C++17 (operator new respects `alignas`). Custom allocators or pre-C++17 heap allocation may need explicit `std::aligned_alloc`.
- `Normalized()` is undefined for zero-length vectors (no guard, for performance).
- `FastInvSqrt` is undefined for `x <= 0`.
- The next natural additions: `Mat4f` (4×4 matrix), `Quaternion`, plane/AABB utilities.
