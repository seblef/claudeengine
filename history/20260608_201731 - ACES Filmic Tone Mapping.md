# ACES Filmic Tone Mapping (Issue #411)

## Summary

Replaced the bare gamma correction in the composite pass with ACES filmic tone mapping and a fixed-exposure pre-scale, giving correct HDR-to-LDR output. The `PostProcessInfos` UBO (slot 10) is introduced to carry `exposure`, `bloom_intensity`, `bloom_threshold`, and `adapt_speed` — the values used by future bloom and eye-adaptation passes.

## Changes

### `data/shaders/glsl/uniforms/post_process_infos.glsl` (new)

New UBO declaration at binding 10.  Four floats packed into a single `vec4` so the struct is exactly one `float4` wide (16 bytes) and satisfies `std140` alignment with no padding.

### `data/shaders/glsl/composite_ps.glsl`

- Added `#include <uniforms/post_process_infos.glsl>` so the composite shader reads `u_exposure` from the new UBO.
- Added the `AcesFilmic()` helper (Krzysztof Narkowicz 2015 approximation).
- Updated `main()`: pre-scale HDR by `u_exposure`, apply ACES, then gamma-correct to sRGB.

### `src/renderer/PostProcessInfos.h` (new)

`alignas(16)` struct with defaults `exposure=1.0`, `bloom_intensity=0.8`, `bloom_threshold=1.0`, `adapt_speed=1.5`.  `static_assert(sizeof==16)` guards the layout.

### `src/renderer/Renderer.h` / `Renderer.cpp`

- Added `post_process_infos_cb_` (UBO slot 10, 1 float4, dynamic).
- Added `post_process_infos_` member holding the initial fixed values.
- CB is bound and filled once per frame in `Update()` alongside the other global CBs.
- Updated the slot-map comment in `Renderer.h` to include slot 10.

## Decisions and rationale

- **UBO over push constants / uniforms**: matches every other per-frame block in the renderer; single `Fill()` call per frame, easy extension.
- **Four fields in one UBO**: bloom and eye-adaptation fields are included now (matching the GLSL uniform block declared in the issue) so future issues can just populate them without touching the struct layout.
- **`u_exposure = 1.0` default**: visually neutral; no change to existing renders until eye adaptation is wired up.
- **ACES approximation**: Narkowicz 2015 rational polynomial — five scalar constants, one `clamp` — fast enough for a fragment shader and well-known in game-engine literature.

## Output to keep in mind

- Slot 10 is now permanently reserved for `PostProcessInfos`; do not assign another UBO to binding 10.
- Issue 4 (eye adaptation) should update `post_process_infos_.exposure` each frame and call `post_process_infos_cb_->Fill()` again (or expose a setter on `Renderer`).
- `bloom_intensity` / `bloom_threshold` / `adapt_speed` are uploaded each frame but not yet consumed by any shader.

## Skills / CLAUDE.md rules applied

- `src/CLAUDE.md`: one class per file, include root is `src/`, Google C++ style guide.
- `data/shaders/glsl/CLAUDE.md`: uniforms declared via `#include`, Blend GLSL coding style, comments for equations.
- Conventional commits message format.
- `cpplint` ran and passed before commit.
