# CLI Texture Compressor (Issue #591)

## Summary

Added a standalone CLI tool `texture_compressor` that converts PNG/JPEG/TGA/BMP images to BCn-compressed DDS files with pre-baked mip chains, and automatically patches material YAML files that reference the converted textures.

## New Files

### `tools/CMakeLists.txt`
Thin wrapper that delegates to `add_subdirectory(texture_compressor)`.

### `tools/texture_compressor/CMakeLists.txt`
Builds the `texture_compressor` executable. Links against:
- `bc7enc_lib` (BC7 encoder, compiled from bc7enc.c)
- `yaml-cpp` (material patching)

Uses include dirs from the already-fetched `stb`, `gli`, and `glm` FetchContent packages.

### `tools/texture_compressor/main.cpp`
Single-file implementation (~450 lines). Key sections:

1. **Format selection** — `AutoDetect()` checks filename for `_normal`/`_nrm`, then falls back to channel count and alpha-channel scan.
2. **Block compression** — `CompressBlock()` dispatches to:
   - `stb_compress_dxt_block(…, 0, …)` for BC1
   - `stb_compress_dxt_block(…, 1, …)` for BC3
   - `stb_compress_bc4_block()` (R-only input) for BC4
   - `stb_compress_bc5_block()` (RG interleaved input) for BC5
   - `bc7enc_compress_block()` for BC7
3. **Mip generation** — iterates `MipCount(w, h)` levels, resizing each with `stbir_resize_uint8_linear(…, STBIR_RGBA)`.
4. **DDS write** — builds a `gli::texture2d` with the right format and mip count, fills it level-by-level, then calls `gli::save_dds()`.
5. **Material patching** — `FindDataRoot()` walks the texture path upward to find the `textures/` component, derives `data_root`, then `PatchMaterials()` recursively scans `data_root/materials/` for `.yaml` files and rewrites matching `rendering.textures.*` values.
6. **Safety validation** — `ValidateTexPath()` rejects absolute paths and paths containing `..`, printing a warning and skipping.

## Changed Files

### `CMakeLists.txt`
- Added **bc7enc** via `FetchContent_GetProperties` + `FetchContent_Populate` (not `MakeAvailable`, to avoid bc7enc's own CMakeLists.txt which creates a conflicting executable target also named `bc7enc`).
- Created `bc7enc_lib` static library from `bc7enc.c`.
- Added `add_subdirectory(tools)` after `add_subdirectory(src)`.

### `data/shaders/glsl/geometry/gbuffer_ps.glsl`
Updated the normal-map decode from full RGB (`texture(u_normal).rgb * 2 - 1`) to RG-only with Z reconstruction:
```glsl
vec2 n_xy = texture(u_normal, v_uv).rg * 2.0 - 1.0;
vec3 tangent_normal = vec3(n_xy, sqrt(max(0.0, 1.0 - dot(n_xy, n_xy))));
```
This is backward-compatible: for a well-formed RGB normal map, the stored B equals the reconstructed Z.

### `data/shaders/glsl/terrain/terrain_ps.glsl`
Updated `DecodeNormal(vec4 sp)` to reconstruct Z from RG instead of reading RGB directly.

### `data/shaders/glsl/water/water_ps.glsl`
Updated the two normal-map sample lines (`n1`, `n2`) to use RG-only decode with Z reconstruction.

## Design Decisions

### FetchContent_Populate vs MakeAvailable for bc7enc
The bc7enc repository has a CMakeLists.txt that creates an executable named `bc7enc` (its test driver). Using `FetchContent_MakeAvailable` causes a CMake name-conflict error when we try to create our own target. Using `FetchContent_Populate` downloads the sources to `external/bc7enc-src/` without running `add_subdirectory`, so we can create `bc7enc_lib` freely.

### stb_dxt for BC1-BC5 (not rgbcx)
bc7enc's repo ships `rgbcx.h` which also handles BC1-5. However, stb_dxt.h already provides `stb_compress_bc4_block` and `stb_compress_bc5_block` (which are exactly what we need), avoiding a new header dependency. bc7enc.c is only compiled for BC7 support.

### Backward-Compatible Shader Change
The Z-reconstruction formula `sqrt(max(0, 1 - x² - y²))` is mathematically equivalent to the stored B channel in any correctly authored normal map, so existing PNG/JPEG normal maps continue to work after the shader change.

### Material Patching Strategy
Only `rendering.textures.*` scalar values are examined. This matches every material file in the project. Paths that are absolute or contain `..` are warned about and skipped. The scan is recursive so subdirectories under `data/materials/` are covered.

## Keep in Mind

- `texture_compressor` is independent of `claude_engine` / `claude_editor` — it can be built with any CMake configuration, no `BUILD_EDITOR=ON` needed.
- bc7enc quality can be tuned via `bc7enc_compress_block_params` (currently using default `bc7enc_compress_block_params_init` which gives a good speed/quality balance). Slowest quality is available via `bc7enc_compress_block_params_init_slowest` if needed.
- BC7 is significantly slower than BC1/BC3/BC4/BC5 and should be used only when maximum quality is needed.
- GLI's `generate_mipmaps.hpp` exists but requires format-aware filtering; we use stb_image_resize2 instead for explicit RGBA mip generation before compression.
- The `--delete` flag removes the source only after a successful `gli::save_dds` call — no partial cleanup.

## Skills / CLAUDE.md Sections Used

- `impl-issue` skill (branch creation, PR workflow)
- Root `CLAUDE.md`: conventional commits, history file, cpplint step, `dev` branch baseline
- `src/CLAUDE.md`: Google C++ style guide conventions applied to tool code
- `data/CLAUDE.md` + `data/shaders/glsl/CLAUDE.md`: shader naming and commenting conventions
