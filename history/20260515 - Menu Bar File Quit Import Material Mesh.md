# Menu Bar: File > Quit, Import > Material/Mesh (Issue #177)

## Changes

### `src/editor/EditorSystem.cpp`
- Added `#include <nfd.h>`.
- `NFD_Init()` called at the end of the constructor (after ImGui setup).
- `NFD_Quit()` called at the beginning of the destructor (before ImGui teardown).

### `src/editor/EditorScene.h` / `EditorScene.cpp`
- Added `#include "game/MeshTemplate.h"`.
- Added `std::vector<game::MeshTemplate*> mesh_templates_` field.
- Added `AddMeshTemplate(game::MeshTemplate*)` inline method — stores the pointer.
- Destructor now calls `tmpl->Release()` on each stored template.

### `src/editor/EditorWindow.h`
- Added `abstract::VideoDevice* video_` field (declared before `scene_` so it initialises first).
- Added three private methods: `RenderMenuBar()`, `ImportMaterial()`, `ImportMesh()`.

### `src/editor/EditorWindow.cpp`
- Added includes: `<filesystem>`, `<nfd.h>`, `<loguru.hpp>`, `EditorSystem.h`, `MeshTemplate.h`, `Material.h`.
- Constructor stores `video_` before initialising children.
- `Render()` now calls `RenderMenuBar()` instead of an inline empty `BeginMainMenuBar` block.
- `RenderMenuBar()`: `BeginMainMenuBar` → File menu (Quit) + Import menu (Material, Mesh) → `EndMainMenuBar`.
- `ImportMaterial()`: `NFD_OpenDialogU8` with `{"Material","yaml"}` filter → `new renderer::Material(path, video_)` → `scene_->AddMaterial(stem, mat)` → log.
- `ImportMesh()`: `NFD_OpenDialogU8` with `{"Mesh","obj,fbx,emesh"}` filter → `MeshTemplate::GetOrLoad(path, video_)` → `scene_->AddMeshTemplate(tmpl)` → log.

## Decisions

- **`NFD_OpenDialogU8` instead of `NFD_OpenDialog`**: The actual nfd-extended header uses `NFD_OpenDialogU8` (UTF-8) / `NFD_OpenDialogN` (native/UTF-16 on Windows). The issue spec referenced an older API. On Linux both are identical (`nfdu8char_t` = `char`), but using the U8 variant is explicit and cross-platform.
- **`NFD_FreePathU8` matching**: Matching free function for the U8 path variant.
- **Material ownership**: `renderer::Material` is heap-allocated and stored in `EditorScene::materials_` (non-owning pointer) — ownership tracking is the caller's responsibility. For imported materials, the `EditorScene` never releases them (same pattern as the default floor/cube materials). This is acceptable for an editor tool; a future milestone can add proper cleanup.
- **Mesh template lifetime**: `MeshTemplate::GetOrLoad` returns an AddRef'd pointer. `EditorScene` stores it in `mesh_templates_` and releases it in the destructor. The template remains in the static registry until ref-count drops to zero, so the Resources panel continues to list it correctly until cleanup.
- **`video_` declaration order**: Declared before `scene_` in the header so it is initialised before the child objects that depend on it in the member-init list.

## Output to keep in mind

- Material importing uses the full absolute path from the dialog; `renderer::Material(string, video)` loads by filename relative to `data/materials/`. If the user picks a file outside `data/materials/`, the path must be either absolute or the constructor must handle it. **Verify this works end-to-end when testing** — may need a path-relative adjustment in a later ticket.
- Imported mesh templates auto-appear in the Resources panel because `MeshTemplate::GetAll()` reads the static registry; no extra wiring needed.
- The Cameras section in the Objects panel remains empty; future `GameCamera` support will populate it.

## Skills used
- `projectSettings:impl-issue`

## CLAUDE.md notes taken into account
- One class per `.h`/`.cpp` pair.
- Editor is the dependency-graph leaf; no new editor includes were added to game/renderer.
- Google C++ style guide; cpplint + cppcheck pre-commit hooks pass.
- Include root is `src/`.
