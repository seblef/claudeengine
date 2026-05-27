# Player Start Flag Gizmo (#316)

## Overview

Makes `GamePlayerStart` objects visible and manipulable in the editor as a coloured flag marker.

## Changes

### New files

- **`src/editor/PlayerStartGizmoRenderer.h/.cpp`** — GPU line-list renderer for the flag gizmo. Mirrors the architecture of `LightWireframeRenderer` but renders **without depth testing** so the gizmo is always visible in front of terrain and meshes. Uses the existing `light_wireframe` GLSL shader (generic coloured line-list). Geometry per player-start object:
  - White vertical pole from the world position up 2 units.
  - Bright-green (`#00CC44`) right-pointing triangular flag at the top of the pole (matches the ▶ visual reference from the issue). The selected object uses a brighter green.
  - Renders into the plain colour FBO (`render_fbo_`), not `wireframe_fbo_`, to bypass the shared GBuffer depth attachment.

### Modified files

- **`src/editor/EditorTool.h`** — Added `kCreatePlayerStart` to the `EditorTool` enum; updated `IsCreationTool()` to include it.
- **`src/editor/EditorToolbar.cpp`** — Added `{EditorTool::kCreatePlayerStart, ICON_FA_FLAG, "Create Player Start", ImGuiKey_None}` to the creation tools strip.
- **`src/editor/ObjectsPanel.cpp`** — Added a "Player Starts" collapsible group (icon: `ICON_FA_FLAG`) rendered below Cameras in the Objects panel tree.
- **`src/editor/EditorViewport.h/.cpp`** — Three additions:
  1. `PlayerStartGizmoRenderer player_start_gizmo_` member; initialised in the constructor alongside `light_wireframe_`.
  2. `SetPendingPlayerStart()` public method — enters placement-preview mode via `BeginPreview(make_unique<GamePlayerStart>(), 0.f, ...)`.
  3. Player-start picking pass in `PickObjectAt()`: screen-space proximity (threshold 12 px) against the projected base position; always wins over any hit (gizmo not depth-tested). Player-start rendering call in `Render()` after the light wireframe pass.
- **`src/editor/EditorWindow.cpp`** — Tool transition code extended to handle `kCreatePlayerStart`: calls `viewport_->SetPendingPlayerStart()` and sets `placement_active_ = true`.
- **`src/editor/CMakeLists.txt`** — Registered `PlayerStartGizmoRenderer.cpp`.

## Key decisions

- **Reuse `light_wireframe` shader** — the shader is a generic pass-through for coloured world-space line lists; no new shader asset was needed.
- **Render into `render_fbo_` (no depth)** — the issue explicitly requires "not depth-tested so it is always visible". Using the plain colour FBO (no GBuffer depth attachment) achieves this without a new render target.
- **Screen-space proximity picking** — `GamePlayerStart` has no mesh geometry, so ray–triangle intersection is not applicable. Picking against the projected world position (threshold 12 px) is consistent with how `GameLight` gizmos are picked.
- **Selection highlight** — selected player start uses a brighter green flag, consistent with how `LightWireframeRenderer` changes colour when selected (yellow → blue).
- **Cancellation** — `SetPendingLightType(std::nullopt)` already calls `CancelPreview()` internally, which is type-agnostic and idempotent; the existing cancel path in `EditorWindow` continues to work for player-start previews as well.

## Output to remember

- The `light_wireframe` shader accepts `VertexBase` (position + colour + UV); uv is unused and set to `{0,0}`.
- `render_fbo_` is the FBO without depth — safe for always-on-top overlays. `wireframe_fbo_` shares the GBuffer depth — use for depth-tested gizmos (lights).
- Player-start picking uses the base position (pole foot), not the flag tip, because that is the world-transform origin.
- The flag triangle is oriented in the XZ plane along the +X axis; if oriented camera-facing is ever needed, the geometry would need to be rebuilt each frame with the camera right vector.

## Skills and instructions applied

- `impl-issue` skill (this contribution)
- `src/CLAUDE.md`: one class per file, Google C++ style, project-relative includes, `cppcheck-suppress unusedStructMember` on private members.
- `src/editor/CLAUDE.md`: editor is the leaf of the dependency graph, ImGui calls bracketed, subsystems not singletons.
