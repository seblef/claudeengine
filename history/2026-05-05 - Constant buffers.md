# Constant / Uniform Buffers

**Date:** 2026-05-05
**Branch:** feat/constant-buffers
**Issue:** #40

## Summary

Added support for constant (DirectX) / uniform (OpenGL) buffers. The abstract `ConstantBuffer` class exposes `Bind()` and `Fill()`. The concrete `GLConstantBuffer` wraps an OpenGL UBO. The `passthrough_color_2d` fragment shader now reads its output color from a UBO rather than vertex attributes. The demo animates the quad color from plain blue to plain red and back over a 10-second sinusoidal cycle.

## Changes

### New files
- `src/abstract/ConstantBuffer.h` — abstract class: constructor (size in float4 units, slot, usage), `Bind()`, `Fill(data)`, getters
- `src/gldevices/GLConstantBuffer.h` — concrete class owning `GLuint ubo_`
- `src/gldevices/GLConstantBuffer.cpp` — constructor allocates GPU buffer via `glBufferData`; `Bind()` calls `glBindBufferBase`; `Fill()` calls `glBufferSubData`

### Modified files

**`src/abstract/VideoDevice.h`**
- Added `#include "abstract/ConstantBuffer.h"`
- Added `CreateConstantBuffer(int size, int slot, BufferUsage usage = kDynamic, const void* data = nullptr)` pure virtual

**`src/gldevices/GLVideoDevice.h` / `.cpp`**
- Added `CreateConstantBuffer` override: constructs `GLConstantBuffer`, optionally fills
- Added `#include "gldevices/GLConstantBuffer.h"`

**`src/gldevices/CMakeLists.txt`**
- Added `GLConstantBuffer.cpp` to the static library sources

**`data/shaders/glsl/passthrough_color_2d_ps.glsl`**
- Added `layout(std140, binding = 0) uniform ColorBlock { vec4 tint_color; }`
- `main()` now outputs `tint_color` instead of the interpolated vertex color

**`src/app/main.cpp`**
- Added `#include <chrono>`, `<cmath>`, `"abstract/ConstantBuffer.h"`
- Creates `cb = video->CreateConstantBuffer(1, 0)` (1 float4, slot 0)
- Calls `cb->Bind()` once after creation
- Each frame: computes `lerp_t` via `-cos` oscillation, lerps `kBlue` → `kRed` → `kBlue` over 10s, calls `cb->Fill(&tint)`
- Clears to black so the animated quad color is visible

## Design Decisions

### `size` in float4 units
Constant buffers are naturally sized in float4 (16-byte) chunks in both DirectX and GLSL `std140` layout. Expressing size in float4 units avoids raw byte arithmetic at call sites. Byte size is `size * 4 * sizeof(float)` computed internally.

### `Fill(data)` — no size or offset parameters
Fills the entire buffer from `size_`. Partial updates would require offset/size parameters but are not needed yet and can be added later.

### `BufferUsage::kDynamic` as default
Constant buffers are almost always updated per-frame. The default removes boilerplate for the common case; callers that need immutable data pass `kImmutable` explicitly.

### `Bind()` uses `glBindBufferBase`
Associates the UBO with its binding slot persistently in OpenGL state. One call is sufficient unless the slot changes between draws.

### `std::chrono` for timing
Avoids adding a GLFW include to `main.cpp`. `steady_clock` is monotonic and correct for animation timing.

### `-cos` oscillation
`lerp_t = (1 - cos(t * π / 5)) / 2` gives a smooth 0→1→0 cycle over exactly 10 seconds: starts at 0 (blue), peaks at 1 (red) at t=5s, returns to 0 (blue) at t=10s.

## Next Steps / Output to Remember

- The vertex shader still outputs `v_color` (unused by the fragment stage). No GL error — unused outputs are silently discarded. If needed, the vertex shader can be cleaned up.
- The `binding = 0` in the GLSL `uniform ColorBlock` matches `slot = 0` passed to `CreateConstantBuffer`. Future shaders with multiple UBOs must use distinct binding points.
- `core::Color` is `alignas(16)` with 4 floats = exactly 1 float4 = matches `std140 vec4` layout with no padding.

## Skills and Instructions Used

- `impl-issue` skill
- `CLAUDE.md`: git workflow, conventional commits, history file, PR
- `src/CLAUDE.md`: Google C++ style, one class per file, include from `src/`
- `data/shaders/glsl/CLAUDE.md`: Blend GLSL coding style, `_vs`/`_ps` suffix convention
