# MeshPreview and PreviewRenderer

**GitHub issue**: #207  
**Branch**: feat/mesh-preview  
**Milestone**: Game materials

## Summary

Implemented `renderer::PreviewRenderer` — an isolated deferred renderer that writes a single mesh + single light into its own offscreen FBO — and `editor::MeshPreview` — an ImGui component that shows a `game::MeshTemplate` in a 256×256 preview with orbit-camera mouse controls. `MaterialEditorWindow` is updated to display the preview.

## New / changed files

- `src/renderer/PreviewRenderer.h` — new non-singleton deferred renderer
- `src/renderer/PreviewRenderer.cpp` — full GBuffer → lighting → emissive → composite pipeline
- `src/editor/MeshPreview.h` — new editor component (orbit camera + ImGui::Image)
- `src/editor/MeshPreview.cpp` — implementation
- `src/editor/MaterialEditorWindow.h` — added `VideoDevice*` constructor param; added `MeshPreview preview_` member; added `~MaterialEditorWindow()` declaration
- `src/editor/MaterialEditorWindow.cpp` — integrate preview; look up `"__proc_editor_cube"` as default geometry
- `src/editor/EditorWindow.cpp` — pass `video_` to `MaterialEditorWindow` constructor
- `src/renderer/CMakeLists.txt` — added `PreviewRenderer.cpp`
- `src/editor/CMakeLists.txt` — added `MeshPreview.cpp`

## Changes

### `src/renderer/PreviewRenderer`

**Not a singleton.** Owns:
- `GBuffer gbuffer_` — 3-MRT FBO at preview resolution
- `EmissiveFBO emissive_fbo_` — HDR accumulation FBO (borrows gbuffer depth RT)
- `scene_infos_cb_` — UBO slot 2, filled with preview camera matrices
- `composite_shader_` + `composite_quad_` — gamma-correct HDR to output RTG

`Render(time, camera, mesh_instance, light, output_rtg)`:
1. Calls `MeshRenderer::EndRender()` + `LightRenderer::EndRender()` to clear any stale lists.
2. Fills and binds `scene_infos_cb_` to UBO slot 2 (overrides main Renderer's until the next `Renderer::Update()` rebinds it).
3. Runs the 4-stage deferred pipeline (geometry → lighting → emissive → composite) using its own GBuffer and EmissiveFBO.
4. Uses the singleton `MeshRenderer`/`LightRenderer` for draw calls; those call `Renderer::Instance().SetRenderableInfos()` and `Renderer::Instance().GetMaterialInfosCB()`, which write to the main Renderer's CBs (slots 1 and 3). Since those CBs are still bound to their respective slots from the previous `Renderer::Update()`, the data reaches the GPU correctly.

### `src/editor/MeshPreview`

Orbit camera state: `azimuth_deg_` (default 45°), `elevation_deg_` (default 30°), `distance_` (1.5 × bbox diagonal). Camera target is the mesh bounding-box center.

`SetTemplate(MeshTemplate*)`:
- Stores the template pointer.
- Reads `GetLocalBBox()`, computes `bbox_diag_` and `center_`, sets `distance_ = 1.5 × bbox_diag_`.
- Creates `MeshInstance` with identity world matrix and `always_visible = true`.

`Render(time)`:
- Calls `UpdateCamera()` → `camera_.SetPosition(center_ + orbit_offset)` + `UpdateMatrices()`.
- Calls `preview_renderer_->Render(...)`.
- Calls `ImGui::Image(color_rt_->GetNativeHandle(), ...)` with Y-flip UVs (OpenGL FBO origin is bottom-left).
- On hovered item: left-drag updates azimuth/elevation, scroll updates distance (clamped to `[0.5, 5] × bbox_diag_`).

### `src/editor/MaterialEditorWindow`

Constructor now takes `abstract::VideoDevice*` to create the embedded `MeshPreview`. On construction, looks up `game::MeshTemplate::Get("__proc_editor_cube")` for the default preview geometry. `Render()` calls `preview_.Render(ImGui::GetTime())`.

## Decisions

- **PreviewRenderer reuses singleton MeshRenderer/LightRenderer**: The batch renderers and their shaders are expensive to duplicate. Since `PreviewRenderer::Render()` is called after `Renderer::Update()` completes (queues are empty), there is no interference. The main Renderer's slots 1 and 3 CBs remain correctly bound; only slot 2 (scene info / camera) is overridden by the preview.
- **`always_visible = true` for preview MeshInstance**: The preview mesh is not registered with any visibility system (we call `Enqueue()` directly), so this flag is irrelevant to culling. It is set defensively.
- **`~MeshPreview()` and `~MaterialEditorWindow()` declared out-of-line**: Both classes store `unique_ptr` members whose complete types are only included in the `.cpp` files. Declaring an explicit out-of-line destructor prevents the compiler from generating inline destructors in unrelated translation units that only see forward declarations.
- **Default preview geometry `"__proc_editor_cube"`**: The cube is created by `EditorScene` before `MaterialEditorWindow`. If the lookup fails (e.g., `EditorScene` not yet constructed), `SetTemplate(nullptr)` leaves the preview empty — no crash. Issue #208 will add user-controlled geometry selection (cube / sphere buttons).

## Known limitation

`LightRenderer::RenderGlobalLights()` sets `cast_shadow = ShadowRenderer::HasCSM() ? 1.0f : 0.0f` regardless of the light's own `cast_shadow` flag. When the main editor scene has a GlobalLight with CSM enabled (the default), the preview GlobalLight will also attempt to use the main scene's cascade shadow maps, producing incorrect shadow patterns in the preview. Fixing this would require changes to `LightRenderer`. Accepted for this PR; the material colors and textures remain clearly visible.

## Output to keep in mind

- `MeshPreview::SetTemplate(MeshTemplate*)` / `Render(float)` are the public API; Issue #208 can change the preview geometry dynamically (cube/sphere selector buttons).
- `PreviewRenderer::Render()` must be called **after** `renderer::Renderer::Update()` in the same ImGui frame (MeshRenderer/LightRenderer queues must be clear).
- The preview light direction is `normalize(-1, -2, -1)`, color `(0.9, 0.85, 0.7)`, intensity `1.0`, ambient `(0.05, 0.05, 0.05)`.

## Skills / CLAUDE.md instructions used

- `src/CLAUDE.md`: one class per `.h`/`.cpp` pair, Google C++ style, include root is `src/`.
- `src/editor/CLAUDE.md`: one class per `.h`/`.cpp` pair; editor is the dependency-graph leaf; all ImGui calls between `NewFrame()` and `Render()`.
- Project `CLAUDE.md`: conventional commits, PR to `dev`, history file required.
