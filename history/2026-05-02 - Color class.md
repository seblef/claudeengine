# Color class and algebra (Issue #7)

## Changes

### `src/core/Color.h` (new)

RGBA floating-point color class with full arithmetic algebra and SSE4.2 SIMD.

**Public API:**

| Member | Description |
|--------|-------------|
| `float r, g, b, a` | Public components. Default: `{0, 0, 0, 1}` (opaque black) |
| `kBlack`, `kWhite`, `kTransparent` | Static constants |
| `Color(float r, float g, float b, float a=1)` | RGBA constructor, alpha defaults to 1 |
| `Color(float scalar)` | Broadcast scalar to all 4 components |
| `+`, `-`, `*`, `/` (Color × Color) | Component-wise arithmetic |
| `*`, `/` (Color × float) | Scalar arithmetic; `float * Color` free function for commutativity |
| unary `-` | Negation (useful for HDR/negative contributions) |
| `+=`, `-=`, `*=`, `/=` (Color and scalar variants) | In-place arithmetic |
| `==`, `!=` | Exact component comparison |
| `Lerp(other, t)` | Linear interpolation |

### `tests/core/CMakeLists.txt`

Added `ColorTest.cpp` to `core_tests`.

### `tests/core/ColorTest.cpp` (new)

26 tests covering all operators and the Lerp method.

**Total: 145 tests, all passing (119 pre-existing + 26 new).**

---

## Decisions and rationale

### Mirrors Vec4f layout

`Color` uses `alignas(16)` and stores `r, g, b, a` as 4 consecutive floats, enabling `_mm_load_ps(&r)` exactly like `Vec4f`. All SIMD operators use the same `Reg()` / `Color(__m128)` bridge pattern.

### Default alpha = 1

`float a = 1.f` makes `Color{}` an opaque black — the most useful "zero colour" for rendering. This differs from `Vec4f` (all zeros) because a fully transparent black is rarely the intended default.

### No clamping

Components are not clamped to [0, 1]. The issue explicitly notes HDR support. Clamping belongs in a conversion step (e.g., tone-mapping or writing to a `uint8_t` buffer), not in the core algebra.

### `operator*` is Hadamard (component-wise)

Colour × colour multiplication is always component-wise in rendering (tinting, modulation). There is no concept of a "matrix-style" or "cross product" multiplication for colours.

### Comparison is exact

`operator==` uses exact float equality, consistent with `Vec4f`. Near-equality comparisons belong at the call site using `NearlyEqual` when needed.

## Output to keep in mind

- `Color{}` is opaque black `{0, 0, 0, 1}`, not fully transparent.
- The `operator*` between two Colors is Hadamard (component-wise), used for lighting modulation.
- HDR values (outside [0, 1]) are valid; tone-mapping/clamping is the caller's responsibility.

## Skills and guidance followed

- `src/CLAUDE.md`: one class per header, `[[nodiscard]]` on const methods, Google C++ style guide
- Root `CLAUDE.md`: branch/PR workflow, history file, conventional commits
