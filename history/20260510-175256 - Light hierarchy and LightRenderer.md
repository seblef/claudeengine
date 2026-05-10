# Light hierarchy and LightRenderer

## Summary

Implements issue #83 (light class hierarchy, volume geometry, LightRenderer singleton with two-pass stencil rendering) as part of the deferred shading milestone.

## New files

### `src/renderer/LightInfos.h`
80-byte GPU constant buffer struct for UBO slot 4. Packs vec3+float per row as plain C++ floats (not `core::Vec3f`) because `Vec3f` is `alignas(16)` with sizeof=16, which would not match the std140 layout where a float follows a vec3 at offset vec3+12 rather than vec3+16.

Static asserts verify all field offsets match the GLSL std140 layout.

### `src/renderer/Light.h/.cpp`
Abstract base. Inherits `Renderable` for spatial placement. Adds `LightType` enum (kGlobal/kOmni/kCircleSpot/kRectSpot) with values matching the `type` float in `LightInfos`. `Enqueue()` submits to `LightRenderer::Instance()`.

### `src/renderer/GlobalLight.h/.cpp`
Always visible (infinite AABB, `always_visible=true`). Stores `ambient_color_` and `direction_` as `Vec3f`. `GetVolumeMatrix()` returns identity; LightRenderer uses a fullscreen quad for this type.

### `src/renderer/OmniLight.h/.cpp`
Local AABB = sphere `[-radius,radius]³`. World matrix = translation (position only). `GetVolumeMatrix()` extracts position from column 3 of the world matrix and returns `Translation(pos) × Scale(radius)`.

### `src/renderer/CircleSpotLight.h/.cpp`
Stores `inner_angle_`, `outer_angle_`, `range_`, `direction_`. Local AABB = conservative sphere of radius `range_`. `GetVolumeMatrix()` builds `Translation(pos) × AlignZToDir(direction) × Scale(range×tan(outer), range×tan(outer), range)`.

`AlignZToDir` is a file-private helper (anonymous namespace) that builds an orthonormal basis from the forward vector and returns the rotation matrix mapping +Z → direction.

### `src/renderer/RectangleSpotLight.h/.cpp`
Same as CircleSpotLight but with independent `h_angle_` and `v_angle_`, scaling the pyramid's X and Y axes independently: `Scale(range×tan(h), range×tan(v), range)`.

### `src/renderer/LightRenderer.h/.cpp`
Singleton (inherits `core::Singleton<LightRenderer>`). Created by `new LightRenderer(video)` and destroyed by `LightRenderer::Shutdown()` (to be wired in Renderer in issue #87).

**Volume meshes** built once at construction:
- `quad_`: 2 triangles covering NDC `[-1,1]²` at z=0 (GlobalLight fullscreen pass).
- `sphere_`: UV sphere, 32 stacks × 64 sectors (2145 vertices, 4096 triangles, all within uint16_t limit).
- `cone_`: 32-segment cone, apex at origin, base at z=1 with radius=1 (45° half-angle, scaled per light).
- `pyramid_`: 5-vertex pyramid, apex at origin, base `±0.5` at z=1 (6 triangles).

**Shaders** (not yet on disk — created in issue #84):
- 1 VS: `lighting/light_pass_vs`
- 4 PS: `lighting/global_light_ps`, `lighting/omni_light_ps`, `lighting/circle_spot_ps`, `lighting/rect_spot_ps`

**Rendering**:
- Instances sorted by `GetType()` so GlobalLights execute first.
- GlobalLight: `SetDepthTestEnabled(false)`, `SetStencilTestEnabled(false)`, `SetFaceCulling(kNone)` — single draw per instance with `global_ps_`.
- Local lights (Omni/Circle/Rect) — two sub-passes per light:
  - Sub-pass A (stencil fill): `SetColorWriteEnabled(false)`, `SetFaceCulling(kNone)`, stencil ALWAYS, front dpfail→INCR_WRAP, back dpfail→DECR_WRAP.
  - Sub-pass B (lighting): `SetColorWriteEnabled(true)`, `SetFaceCulling(kBack)`, `SetDepthTestEnabled(false)`, stencil NOT_EQUAL 0.

**`FillInfos()`** fills `LightInfos` from each light subclass using a type-switch + static_cast. All four fields per type are populated.

### `data/shaders/glsl/uniforms/light_infos.glsl`
GLSL std140 UBO binding 4 with the same layout as `LightInfos`.

## Decisions

- **Separate `LightType` enum** (not bitmask): ordering is meaningful for render-time sorting; values are cast to float for the `LightInfos.type` field.
- **`Vec3f` not used in `LightInfos`**: std140 packs `vec3+float` into 16 bytes, but C++ `Vec3f` is sizeof=16 by itself due to `alignas(16)` and the `w_` padding field. Plain floats are used to keep the sizes matching.
- **`AlignZToDir` duplicated** in CircleSpotLight.cpp and RectangleSpotLight.cpp (anonymous namespace in each TU): avoids a shared utility header for a 10-line function used in exactly two places.
- **Conservative AABB for spot lights**: local bbox is `[-range,range]³` (sphere of radius range), not the exact cone/pyramid. Acceptable for a first implementation; can be tightened later.
- **No `SetBlendEnabled` in VideoDevice**: blend state is not yet in the abstract API (to be added in issue #87 when the full 4-phase pipeline is wired). Additive blend for lights is a TODO at that stage.

## Output to keep in mind

- LightRenderer must be instantiated (`new LightRenderer(video_)`) and shut down (`LightRenderer::Shutdown()`) in `Renderer.cpp` — deferred to issue #87.
- The 4 lighting PS shaders and the shared VS do not exist yet — created in issue #84.
- `GetVolumeMatrix()` for spot lights extracts position from `GetWorldMatrix()(row,3)` — if the user sets a non-translation world matrix (e.g. includes rotation/scale), the position extraction will be incorrect. Document or enforce that the world matrix for local lights must be a pure translation.
