# Shadow Renderer Skeleton (Issue #145)

## Changes

### `src/renderer/IVisibilitySystem.h`
Added pure virtual `CullAndCollect(frustum, out)` alongside the existing `CullAndEnqueue`. The key design decision: `CullAndCollect` has no side effects — it only appends to a caller-owned vector. This separation is essential because shadow-pass culling uses each *light's* frustum independently, which is a different frustum from the main camera. `CullAndEnqueue` enqueues into global renderer queues and cannot be reused for per-light culling.

### `src/renderer/NoCullingVisibilitySystem.h/.cpp`
Implemented `CullAndCollect` to append all registered renderables (frustum ignored). Consistent with `CullAndEnqueue` semantics: always-visible objects must always appear as shadow casters regardless of the light's frustum.

### `src/renderer/OctreeVisibilitySystem.h/.cpp`
Added `CullAndCollect` override and private `CollectNode` helper. `CollectNode` mirrors `CullNode` but appends to `std::vector<Renderable*>` instead of calling `Enqueue()`. DFS traversal with `ContainsBBox` frustum test is identical — shadow culling reuses the same spatial structure.

### `src/renderer/Renderable.h`
Added two virtual methods with default no-op implementations:
- `IsShadowCaster() const → false`: lights and non-mesh renderables correctly return false without any override.
- `DrawDepth(VideoDevice*)`: no-op base, mesh subclasses override to submit geometry depth-only.

Forward-declared `abstract::VideoDevice` to avoid a heavyweight include in the base class header.

### `src/renderer/MeshInstance.h`
Overrides `IsShadowCaster()` to return `true` unconditionally. Issue #146 will change this to delegate to `material_->GetCastShadow()` once that material flag is added.

### `src/renderer/RenderableMeshInstance.h/.cpp`
- `IsShadowCaster()`: returns true if any sub-instance is a shadow caster (short-circuits on first true).
- `DrawDepth()`: iterates sub-instances, skips non-casters, calls `geo->Set()` + `video->RenderIndexed()` on each. No material binding — depth-only pass.

### `src/renderer/LightRenderer.h`
Added `GetLights() const` accessor exposing `instances_`. Called by `Renderer::Update()` to pass the enqueued light list to `ShadowRenderer::RenderShadowMaps()` after `CullAndEnqueue` fills the queue.

### `src/renderer/ShadowPassInfos.h` (new)
64-byte (4 float4s) struct containing `light_vp: Mat4f`. Bound at CB slot 6 during the shadow pass. Static assert verifies the size.

### `src/renderer/ShadowMap.h/.cpp` (new)
Owns:
- `kDepth32F` `RenderTarget` (hardware PCF-capable via `GL_COMPARE_REF_TO_TEXTURE`).
- Depth-only `RenderTargetGroup` (empty color span → `glDrawBuffer(GL_NONE)`).
- Cached `light_vp` matrix updated each frame before the shadow pass.

Constructor takes `(VideoDevice*, int resolution)`. Exposes `GetDepthRT()`, `GetFBO()`, `GetLightVP()`, `SetLightVP()`, `GetResolution()`.

### `src/renderer/ShadowRenderer.h/.cpp` (new)
Singleton (CRTP `core::Singleton<ShadowRenderer>`). Key implementation decisions:

**Light-type filtering**: GlobalLight (CSM) and OmniLight (cube maps) are skipped for now — they require more complex infrastructure in later issues.

**Light VP computation** (`ComputeLightVP`):
- Position extracted from `world_matrix(row, 3)` columns — row-major layout.
- `CircleSpotLight`: `fov_y = 2 * outer_angle`, `aspect = 1.0`.
- `RectangleSpotLight`: `fov_y = 2 * v_angle`, `aspect = tan(h_angle) / tan(v_angle)`.
- Up vector: `(1,0,0)` when `|dir.y| > 0.9`, else `(0,1,0)` — avoids collinearity at vertical directions.
- Uses `LookAtRH` + `PerspectiveRH` (OpenGL NDC convention).

**Shadow pass per light**:
1. Allocate `ShadowMap` on first use, cached in `unordered_map<const Light*, unique_ptr<ShadowMap>>`.
2. Compute and store `light_vp`, upload to CB slot 6.
3. Build `ViewFrustum` from `light_vp`, collect from both visibility systems.
4. Bind FBO, set viewport to shadow map resolution, clear depth.
5. Front-face culling during shadow pass (reduces shadow acne without explicit bias).
6. Iterate casters, filter by `IsShadowCaster()`, call `DrawDepth()`.
7. Restore back-face culling and full-screen viewport after all lights.

**CB slot 6**: `ShadowPassInfosBlock` — only bound during the shadow pass, not during the main geometry/lighting passes.

### Depth-only shaders (new)
- `data/shaders/glsl/shadow_depth_vs.glsl`: transforms `in_position` by `light_vp * world`, outputs only `gl_Position`.
- `data/shaders/glsl/shadow_depth_ps.glsl`: empty body — depth written implicitly by rasterizer.
- `data/shaders/glsl/uniforms/shadow_pass_infos.glsl`: `ShadowPassInfosBlock` at `binding = 6`.

### `src/renderer/Renderer.h/.cpp`
- Constructor: `new ShadowRenderer(video_)`.
- Destructor: `ShadowRenderer::Shutdown()` before `LightRenderer::Shutdown()`.
- `ClearVisibilitySystems()`: also calls `ShadowRenderer::Instance().ClearShadowMaps()`.
- `Update()`: shadow pass (step 0) runs after `CullAndEnqueue` fills the light queue, before the geometry pass. Lights are obtained from `LightRenderer::Instance().GetLights()`.

### `src/renderer/CMakeLists.txt`
Added `ShadowMap.cpp` and `ShadowRenderer.cpp`.

## Key Design Decisions

**Why `CullAndCollect` instead of reusing `CullAndEnqueue`**: Shadow culling needs to run per-light against a different frustum, and must not enqueue objects into the main render queue. The separation keeps both systems side-effect-free when appropriate.

**Why front-face culling in shadow pass**: Culling front faces during shadow rendering moves the self-shadowing bias problem to back faces, which are geometrically correct from a depth perspective. This avoids explicit depth bias tuning for now.

**Why skip `GetCastShadow()` for now**: The material flag is added in issue #146. `MeshInstance::IsShadowCaster()` returns `true` unconditionally as a placeholder — the comment in the code marks the update point.

**Why `unordered_map` for shadow map cache**: Light pointers are stable across frames (no per-frame allocation), so pointer identity is a valid key. Maps are cleared on scene reset via `ClearShadowMaps()`.

## Notes for Future Issues

- Issue #146: Change `MeshInstance::IsShadowCaster()` to return `material_->GetCastShadow()`.
- Issue #148 (or later): `OmniLight` shadow rendering requires `RenderTargetCube` (6-face cube map). The `kOmni` type is explicitly skipped in `ShadowRenderer::RenderShadowMaps()` with a comment.
- Issue #147 or CSM issue: `GlobalLight` shadow requires Cascaded Shadow Maps. Also skipped with a comment.
- Shadow map pool (later issue): Replace the naive one-map-per-light allocation with a tiered pool to cap GPU memory usage.

## Skills Used
- `impl-issue` skill
