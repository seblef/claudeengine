# MeshTemplate and GameMesh: GameMaterial integration

**GitHub issue**: #205  
**Branch**: feat/mesh-template-game-material  
**Milestone**: Game materials

## Summary

Refactored `game::MeshTemplate` to own GPU geometry and hold an AddRef'd `game::GameMaterial*` instead of a raw `renderer::Material`. Added `SetMaterial()` for O(1) live material swaps. Added `GetTemplate()` to `game::GameMesh`. Updated all procedural mesh creation sites (`main.cpp`, `EditorScene.cpp`) to use the new constructor.

## New / changed files

- `src/game/MeshTemplate.h` — redesigned header
- `src/game/MeshTemplate.cpp` — redesigned implementation
- `src/game/GameMesh.h` — added `GetTemplate()`
- `src/game/GameMesh.cpp` — implemented `GetTemplate()`
- `src/app/main.cpp` — updated floor plane and material assignment
- `src/editor/EditorScene.cpp` — updated floor and cube template creation

## Changes

### `src/game/MeshTemplate.h` / `MeshTemplate.cpp`

**Removed:**
- `std::unique_ptr<renderer::Material> material_` and its associated `#include "renderer/Material.h"`.
- The old `MeshTemplate(geo, mat)` procedural constructor (took `unique_ptr<renderer::Material>`).

**Added:**
- `game::GameMaterial* material_` (forward-declared in header via `namespace game { class GameMaterial; }`; full include in `.cpp`).
- Destructor: calls `material_->Release()`.
- New procedural constructor `MeshTemplate(id, geo, mat)` — takes explicit string id (use `"__proc_"` prefix to exclude from `GetAll()`), owned geometry, and AddRef'd `GameMaterial*`.
- `void SetMaterial(GameMaterial* mat)` — releases old ref, AddRef's new, calls `mesh_->SetMaterial(mat->GetMaterial())`. All live `MeshInstance`s see the new material on the next frame.
- `renderer::GeometryData* GetGeometry() const` and `GameMaterial* GetMaterial() const` accessors.

**File-backed constructor** creates a default `GameMaterial` keyed `"__mat_" + mesh_path` via `GameMaterial(name, MaterialDesc(), video)`. This is an internal, unnamed-in-the-sense-of-the-game material that callers can replace via `SetMaterial()`.

### `src/game/GameMesh.h` / `GameMesh.cpp`

Added `GetTemplate() const → MeshTemplate*` — the material editor will use this to call `SetMaterial()` on the template backing a selected mesh.

### `src/app/main.cpp`

- Removed `#include "renderer/Material.h"` (no longer directly used).
- Floor plane: replaced `std::make_unique<renderer::Material>(...)` with `new game::GameMaterial("__proc_floor", MaterialDesc(...), video)`, passed to `MeshTemplate("__proc_floor", geo, mat)`. `floor_mat->Release()` called immediately after since `plane_tmpl` AddRef'd it.
- Demo mesh material assignment changed from `tmpl->GetMesh()->SetMaterial(demo_mat->GetMaterial())` to `tmpl->SetMaterial(demo_mat)` — cleaner and ref-correct.

### `src/editor/EditorScene.cpp`

- Replaced `#include "renderer/Material.h"` with `#include "game/GameMaterial.h"`.
- Floor and cube templates created with `GameMaterial` + new `MeshTemplate(id, geo, mat)` constructor.
- `materials_["floor_grey"]` and `materials_["default"]` still populated with `renderer::Material*` via `game_mat->GetMaterial()` for backward compatibility with `EditorScene.h`'s map type (Issue #206 will update this map to store `GameMaterial*`).

## Decisions

- **Forward declaration in header, full include in `.cpp`**: `MeshTemplate.h` only needs `GameMaterial*` (pointer), so a forward declaration avoids pulling in `<yaml-cpp/yaml.h>` and other `GameMaterial.h` transitive deps into every translation unit that includes `MeshTemplate.h`.
- **File-backed constructor creates an internal default `GameMaterial`**: keyed `"__mat_" + mesh_path`. This is released (via ref-counting) when `SetMaterial()` is called with a real material. No persistent side effect.
- **`"__proc_"` id convention**: procedural templates use ids starting with `"__proc_"` to be excluded from `MeshTemplate::GetAll()`, which the resources panel uses. This is enforced by convention, not by a separate constructor overload.
- **`EditorScene.h` materials map stays `renderer::Material*`**: issue #206 will update `EditorScene` and `EditorWindow` to use `GameMaterial` throughout. Changing the map type now would be out of scope and break `EditorWindow.cpp`.

## Output to keep in mind

- `MeshTemplate::SetMaterial(GameMaterial*)` is the canonical way to change a mesh's material at runtime. `renderer::Mesh::SetMaterial()` should not be called directly from game or editor code.
- `GameMesh::GetTemplate()` exposes the template for the material editor to call `SetMaterial()` on.
- Issue #206 will change `EditorScene.h`'s `materials_` map from `renderer::Material*` to `game::GameMaterial*` and update `EditorWindow` to import materials as `GameMaterial`.
- Procedural `GameMaterial` instances used for floor/cube are keyed `"__proc_editor_floor"` / `"__proc_editor_cube"` / `"__proc_floor"` in the `GameMaterial` registry. These are valid resource registry entries but are not visible to the resources panel.

## Skills / CLAUDE.md instructions used

- `src/CLAUDE.md`: one class per `.h`/`.cpp` pair, Google C++ style, include root is `src/`.
- `src/game/CLAUDE.md`: one class per `.h`/`.cpp` pair, no platform/OpenGL headers.
- Project `CLAUDE.md`: conventional commits, PR to `dev`, history file required.
