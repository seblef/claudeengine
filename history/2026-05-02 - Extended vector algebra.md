# Extended vector algebra (Issue #5)

## Changes

### `src/core/Vec2f.h`, `src/core/Vec3f.h`, `src/core/Vec4f.h`

8 new methods added to each class in the vector operations section:

| Method | Signature | Notes |
|--------|-----------|-------|
| `SquaredDistance` | `float (const VecXf&) const` | `(other - *this).LengthSquared()` |
| `Distance` | `float (const VecXf&) const` | `sqrt(SquaredDistance)` |
| `IsBetween` | `bool (const VecXf& a, const VecXf& b, float eps = 1e-5f) const` | Geometric segment test |
| `Lerp` | `VecXf (const VecXf&, float t) const` | `*this + (other - *this) * t` |
| `Scale` | `VecXf (const VecXf&) const` | Component-wise multiply |
| `InvScale` | `VecXf (const VecXf&) const` | Component-wise divide |
| `Inverse` | `VecXf () const` | Per-component `1/x` |
| `InverseInPlace` | `VecXf& ()` | `*this = Inverse(); return *this` |

### Test files (new)
- `tests/core/Vec2fTest.cpp` — 15 tests
- `tests/core/Vec3fTest.cpp` — 16 tests (includes `ScalePreservesW`)
- `tests/core/Vec4fTest.cpp` — 15 tests

**Total: 119 tests, all passing (73 pre-existing + 46 new).**

---

## Decisions and rationale

### IsBetween — geometric segment test

The issue says "test if the vector is between two other vectors", which means a point-on-segment test, not component-wise clamping. The implementation uses the triangle inequality:

```
|PA| + |PB| == |AB|  ⟺  P is on segment [A, B]
```

This is dimension-agnostic (works identically for Vec2f, Vec3f, Vec4f), requires no cross product, and handles both the collinearity and parameter-range checks in a single expression. The default epsilon `1e-5f` matches `NearlyEqual` throughout the codebase.

### SIMD for Vec3f Scale/InvScale/Inverse

Vec3f stores a hidden `w_=0` padding float for SIMD alignment.

- **Scale**: `_mm_mul_ps` is safe — `0 * 0 = 0`, invariant preserved.
- **InvScale**: scalar-only — `_mm_div_ps` would yield `0/0 = NaN`, corrupting `w_`.
- **Inverse**: scalar-only — `_mm_div_ps(1, Reg())` would yield `1/0 = inf`, corrupting `w_`.

`Lerp`, `SquaredDistance`, `Distance`, and `IsBetween` all compose existing SIMD operators (`operator-`, `operator*`, `LengthSquared`, `Distance`) and inherit their SIMD benefits without raw intrinsics.

### Vec4f Scale/InvScale/Inverse use full SIMD

Vec4f has no padding; all 4 components are meaningful. `_mm_mul_ps`, `_mm_div_ps`, and `_mm_div_ps(_mm_set1_ps(1.f), ...)` are used directly.

## Output to keep in mind

- `IsBetween(a, b, eps)` is a geometric segment test, not a bounding-box test.
- `InvScale` and `Inverse` on Vec3f are intentionally scalar to protect the `w_=0` invariant. Do not add SIMD there.
- Future Quaternion/geometry work that needs a bounding-box test should implement it separately (e.g., an AABB type), not repurpose `IsBetween`.

## Skills and guidance followed

- `src/CLAUDE.md`: one class per header, Google C++ style guide, `[[nodiscard]]` on const methods
- Root `CLAUDE.md`: branch/PR workflow, history file, conventional commits
