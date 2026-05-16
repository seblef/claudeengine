# Add game::GameMaterial resource

**GitHub issue**: #204  
**Branch**: feat/game-material  
**Milestone**: Game materials

## Summary

Introduced `game::GameMaterial`, a ref-counted resource inheriting from `core::Resource<std::string, GameMaterial>` that owns a `renderer::Material` and acts as the single authority for rendering material state at the game layer. Updated the YAML format for material files to use a top-level `rendering:` section. Migrated all 3 existing material files. Updated `data/materials/CLAUDE.md`.

## New files

- `src/game/GameMaterial.h`
- `src/game/GameMaterial.cpp`

## Changes

### `src/app/main.cpp`

Updated the game app entrypoint to load materials via `GameMaterial::GetOrLoad` and apply them to the demo meshes:

- Added `#include "game/GameMaterial.h"`.
- Calls `GameMaterial::GetOrLoad("demo", video)` after loading mesh templates.
- Calls `tmpl->GetMesh()->SetMaterial(demo_mat->GetMaterial())` on both `obj_tmpl` and `fbx_tmpl` before constructing the `GameMesh` objects, so the demo scene uses the `data/materials/demo.yaml` material (diffuse texture `demo.png`).
- Releases `demo_mat` after the main loop (before `GameSystem::Shutdown()`), ensuring the material remains valid for the entire frame lifetime.
- The floor plane continues to use a raw `renderer::Material` via the procedural `MeshTemplate` constructor; this will be addressed in Issue #205.

### `src/game/GameMaterial.h` / `GameMaterial.cpp`

New class with three constructors:
- `GameMaterial(name, video)` — loads from `data/materials/{name}.yaml`, extracts the `rendering:` sub-node, passes it to `renderer::Material`
- `GameMaterial(name, yaml, video)` — constructs from a pre-parsed YAML root node (same `rendering:` extraction logic)
- `GameMaterial(name, desc, video)` — constructs programmatically from a `MaterialDesc`

Static factory:
- `GetOrLoad(name, video)` — returns the existing instance (AddRef'd) or loads a new one from disk

Accessor:
- `GetMaterial() → renderer::Material*` — always non-null after successful init

### `src/renderer/Material.cpp`

Two changes:

1. **`Material(const std::string& name, video)`**: Updated to navigate into the `rendering:` sub-node when present, so the file-based constructor transparently handles both the new format and legacy flat format.

2. **`Material::LoadFromYaml`**: Added support for flat color keys (`diffuse_color`, `emissive_color`, `ambient_color`, `shininess`) at the YAML node level, matching the new format. The old `colors:` sub-node is retained for backward compatibility with any legacy files imported via the editor's import dialog.

### `src/game/CMakeLists.txt`

Added `GameMaterial.cpp` to the `game` static library source list.

### `data/materials/` — YAML format migration

All 3 existing material files migrated from the old flat format to the new `rendering:` section format:

- `data/materials/demo.yaml`: `textures:` moved under `rendering:`
- `data/materials/demo/bark.yaml`: `colors.diffuse` moved to `rendering.diffuse_color`
- `data/materials/demo/tree.yaml`: `colors.diffuse` moved to `rendering.diffuse_color`

### `data/materials/CLAUDE.md`

Updated to document the new YAML format, explain the `rendering:` section structure, and describe how to load materials via `GameMaterial::GetOrLoad`.

## Decisions

- **`rendering:` sub-node extracted in `GameMaterial`, not `Material`**: `Material` receives the already-extracted sub-node. This keeps `Material` agnostic of the game-layer material structure, while `GameMaterial` owns the top-level format knowledge.
- **Backward compat in `Material::LoadFromYaml`**: The old `colors:` sub-node is preserved alongside the new flat keys, so `EditorWindow`'s direct `Material(path, video)` import path continues to work for any files that haven't been migrated.
- **Name as registry key**: `GameMaterial` is keyed by the material name (basename without extension), consistent with the logical identity of a material rather than its file path. This allows programmatic materials to coexist with file-backed ones under distinct names.
- **Default fallback on missing `rendering:` key**: `GameMaterial` logs a warning and creates a default (untextured) material, rather than failing hard, to avoid crashing on partially-formed files during development.

## Output to keep in mind

- `game::GameMaterial` is the new entry point for loading and managing rendering materials at the game layer. Future code should prefer `GameMaterial::GetOrLoad` over constructing `renderer::Material` directly.
- YAML material files must have a top-level `rendering:` section. The old flat format (no `rendering:` key) is still parsed by `renderer::Material` for backward compatibility only.
- Issue #205 will update `MeshTemplate` and `GameMesh` to reference `GameMaterial` instead of owning a raw `renderer::Material`.
- Issue #206 will connect `GameMaterial` to the editor's `EditorScene` and `ResourcesPanel`.

## Skills / CLAUDE.md instructions used

- `src/CLAUDE.md`: one class per `.h`/`.cpp` pair, Google C++ style, include root is `src/`.
- `src/game/CLAUDE.md`: one class per `.h`/`.cpp` pair, no platform/OpenGL headers.
- Project `CLAUDE.md`: conventional commits, PR to `dev`, history file required.
