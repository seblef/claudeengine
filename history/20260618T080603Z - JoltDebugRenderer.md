# JoltDebugRenderer bridge to WireframeRenderer

**Issue:** #623  
**Branch:** `feat/jolt-debug-renderer-bridge-623`

## Change

New files:
- `src/physics/JoltDebugRenderer.h` — `JPH::DebugRenderer` subclass declaration (only includes `<Jolt/Jolt.h>` and `<Jolt/Renderer/DebugRenderer.h>`; no game/renderer types in the header)
- `src/physics/JoltDebugRenderer.cpp` — full implementation; includes `renderer/WireframeRenderer.h`

Modified files:
- `src/physics/PhysicsSystem.h` — forward-declares `JoltDebugRenderer`, adds `std::unique_ptr<JoltDebugRenderer> debug_renderer_` private member
- `src/physics/PhysicsSystem.cpp` — includes `JoltDebugRenderer.h`, creates instance in `Init()` and sets `JPH::DebugRenderer::sInstance`; clears `sInstance` before destroying in destructor
- `src/physics/CMakeLists.txt` — adds `JoltDebugRenderer.cpp` to sources, adds `renderer` as a PRIVATE link dependency, applies `-fno-rtti` to `JoltDebugRenderer.cpp`

## Key decisions

### Subclassing `JPH::DebugRenderer` directly (not `DebugRendererSimple`)

The issue spec says to include only `<Jolt/Renderer/DebugRenderer.h>`. Subclassing `DebugRendererSimple` would require `DebugRendererSimple.h` and its private `BatchImpl`. Instead, a local `BatchImpl` struct is defined in the `.cpp` file, mirroring what `DebugRendererSimple` does internally. This keeps the header dependency minimal and explicit.

### `<Jolt/Jolt.h>` as first Jolt include

`<Jolt/Renderer/DebugRenderer.h>` doesn't include `<Jolt/Jolt.h>` itself; Jolt requires that header to be the first include in every TU that uses Jolt. Adding `<Jolt/Jolt.h>` to `JoltDebugRenderer.h` before `<Jolt/Renderer/DebugRenderer.h>` makes the header self-contained regardless of include order. In the `.cpp` file `<Jolt/Jolt.h>` is listed first (before the project header) for the same reason.

### `-fno-rtti` per-file flag

Jolt is compiled with `-fno-rtti -fno-exceptions`. A subclass of `JPH::DebugRenderer` must also be compiled without RTTI; otherwise the linker fails with `undefined reference to 'typeinfo for JPH::DebugRenderer'` because no typeinfo was emitted in the Jolt library. The flag is applied only to `JoltDebugRenderer.cpp` via `set_source_files_properties` to avoid forcing `-fno-rtti` on the rest of the physics module.

### `PreScaled(Vec3::sReplicate(1.02f))` for the 1.02× scale

`RMat44::PreScaled(inScale)` scales only the rotation columns (not the translation column), which is exactly `M * Scale(s)` in math: local-space coordinates are magnified by 1.02 while the origin of the shape stays at the same world position. For both `Mat44` (single-precision) and `DMat44` (double-precision) `RMat44` variants, `PreScaled` exists with the same semantics.

### `physics` PRIVATE-links `renderer`

The physics CLAUDE.md previously said physics must not depend on game/renderer/editor/gldevices. This feature deliberately adds a PRIVATE dependency on `renderer` (for `WireframeRenderer::PushSegment`). Since the dependency is PRIVATE, no renderer types leak into physics public headers and there is no circular dependency (renderer does not depend on physics). The physics CLAUDE.md should be updated to document this allowed exception.

### `IsInstanced()` guard in `DrawLine`

`JPH::DebugRenderer::sInstance` is set during `PhysicsSystem::Init()`, but Jolt may call `DrawLine`/`DrawGeometry` at any time after the physics system starts. If `WireframeRenderer` hasn't been created yet (e.g., in a headless test), the guard prevents a null-deref.

### LOD selection: always use `mLODs[0]`

Without camera position tracking, using the first LOD (highest quality) is the safest default for debug visualization. In practice, Jolt creates most geometry with a single LOD at distance `cLargeFloat`, so this is equivalent to the `DebugRendererSimple` fallback.

## Suggested CLAUDE.md updates

### `src/physics/CLAUDE.md`

Add to the **Guidelines** section:

> **Exception:** `physics/JoltDebugRenderer.cpp` PRIVATE-links against `renderer` to forward draw calls to `WireframeRenderer::PushSegment`. No renderer type appears in any physics public header.

## Skills and instructions used

- `impl-issue` skill (workflow: checkout dev, branch, implement, cpplint, commit, PR)
- `src/CLAUDE.md`: one class per .h/.cpp pair; include root is `src/`; Google C++ style guide
- `src/physics/CLAUDE.md`: Jolt types must not appear in physics/*.h included by game/editor
