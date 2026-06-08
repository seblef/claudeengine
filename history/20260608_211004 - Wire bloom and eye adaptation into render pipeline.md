# Wire Bloom and Eye Adaptation into the Render Pipeline

**Issue**: #414
**Branch**: feat/wire-bloom-eye-adapt-pipeline
**Date**: 2026-06-08

## Summary

Integrated `BloomRenderer` and `EyeAdaptationRenderer` into `Renderer`, gated by `PostProcessConfig` flags. Extended the composite shader to sample the bloom texture at slot 11. Added an ImGui post-process panel in the editor.

## Changes

### `src/renderer/Renderer.h`

- Added `#include "renderer/BloomRenderer.h"`.
- Replaced value member `EyeAdaptationRenderer eye_adaptation_` with `std::unique_ptr<EyeAdaptationRenderer> eye_adapt_renderer_`.
- Added `std::unique_ptr<BloomRenderer> bloom_renderer_`.
- Added `std::unique_ptr<abstract::RenderTarget> null_bloom_rt_` — a 1×1 black R11F_G11F_B10F RT bound at sampler slot 11 when bloom is disabled, so the composite shader is uniform in both cases.
- Added `GetPostProcessInfos()` accessor for the editor to read/write `post_process_infos_` directly.

### `src/renderer/Renderer.cpp`

- Added `#include "abstract/TextureFormat.h"` (needed for `kG11R11B10F`).
- Moved `#include "core/AppConfig.h"` into alphabetical order.
- **Constructor**: conditional creation of `bloom_renderer_` and `eye_adapt_renderer_` from `PostProcessConfig`; unconditional creation of `null_bloom_rt_` (1×1 black, cleared to black via a temporary scoped FBO).
- **Destructor**: explicit `Destroy()` calls on both `unique_ptr` renderers before they are released.
- **`Update()`**: removed early `post_process_infos_cb_->Fill()` (CB was filled before eye adaptation updated exposure); added step 5a (eye adaptation), step 5b (bloom), then `post_process_infos_cb_->Fill()` right before the composite pass; bloom texture bound at slot 11 (or null RT when disabled).
- **`OnResize()` / `ResizeTargets()`**: conditional `Resize()` forwarded to both sub-renderers.

### `data/shaders/glsl/composite_ps.glsl`

- Added `layout(binding = 11) uniform sampler2D u_bloom`.
- Updated `main()` to additively blend bloom before tone-mapping: `hdr = (hdr + bloom) * u_exposure`.
- Updated header comment to document the new pipeline step.

### `src/editor/EditorWindow.h`

- Added `bool show_post_process_panel_` flag.

### `src/editor/EditorWindow.cpp`

- Added `#include "core/AppConfig.h"`, `"renderer/PostProcessInfos.h"`, `"renderer/Renderer.h"`.
- Added **View > Post-process** menu item that toggles the panel.
- Added panel 11f: a dockable **Post-process** window with:
  - Exposure slider (0.1–10) shown only when eye adaptation is off.
  - Bloom intensity slider (0–2) shown only when bloom is enabled.
  - Bloom threshold slider (0–3) shown only when bloom is enabled.
  - Adapt speed slider (0.1–5) shown only when eye adaptation is on.

## Decisions

### Value → unique_ptr for sub-renderers

The `EyeAdaptationRenderer` was previously a value member, unconditionally created. Converting to `unique_ptr` matches the bloom renderer design and avoids allocating GPU resources when the config flag is off.

### null_bloom_rt_ in Renderer (not a fallback in shader)

Rather than branching in the shader (`#ifdef BLOOM`) or using a uniform boolean, the composite shader always samples slot 11. When bloom is disabled, a pre-cleared 1×1 black RT is bound there. This keeps the shader simple and avoids driver recompilation or shader variants.

### CB fill moved to just before composite pass

The original code filled `post_process_infos_cb_` at the top of `Update()`, before eye adaptation had a chance to update `exposure`. Moving the fill to after step 5a ensures the correct exposure value (after smoothing) is uploaded to the GPU. This also avoids a redundant double-fill.

### Editor reads PostProcessInfos by reference

`Renderer::GetPostProcessInfos()` returns a mutable reference. The CB is uploaded every frame right before the composite pass, so editor edits made anywhere in the frame take effect on the next composite draw without any explicit "flush" call.

## Output / Notes for Next Features

- `BloomRenderer` and `EyeAdaptationRenderer` both use explicit `Create()` / `Destroy()` lifecycle — they do NOT call `Destroy()` in their destructors. Always call `Destroy()` before dropping the `unique_ptr`.
- The null_bloom_rt_ is a 1×1 R11G11B10F target, consistent with the bloom levels' format.
- Slot 11 is now dedicated to the bloom texture in the composite pass. Other shaders must not use sampler binding 11 in that pass.
- The post-process panel is lightweight; no persistence — values are in-memory `PostProcessInfos` defaults and reset on engine restart. Persistence (e.g., in config.yaml or map file) is left for a future issue if needed.

## Files Used

- `src/renderer/CLAUDE.md`
- `src/editor/CLAUDE.md`
- `data/shaders/glsl/CLAUDE.md`
- `CLAUDE.md` (root)

## Key Instructions Followed

- One class per `.h` / `.cpp` pair.
- Google C++ style guide.
- `cpplint` passed.
- History MarkDown stored in `history/`.
- Conventional commit message.
