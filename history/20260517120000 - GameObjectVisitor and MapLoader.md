# GameObjectVisitor + MapLoader — runtime map loading from .map.yaml

**Issue:** #249  
**Branch:** feat/game-object-visitor-map-loader-249  
**Date:** 2026-05-17

---

## What was done

### Part A — Visitor infrastructure

**New file `src/game/GameObjectVisitor.h`** (header-only):
- Declares `game::GameObjectVisitor` with three pure-virtual overloads: `Visit(GameMesh&)`, `Visit(GameLight&)`, `Visit(GameCamera&)`.
- Forward-declares the three concrete subclasses to avoid circular includes.

**`GameObject` modified** (`src/game/GameObject.h`):
- Added `virtual void Accept(GameObjectVisitor& visitor) = 0;` as a pure-virtual.
- Included `game/GameObjectVisitor.h` in the header (all subclasses pull it transitively).

**Each subclass** (`GameMesh`, `GameLight`, `GameCamera`):
- Added one-liner inline `Accept` override in the class header calling `visitor.Visit(*this)`.

### Part B — MapLoader

**New `src/game/MapLoader.h`**:
- `struct MapData { name, map_size, global_light, objects }` — aggregates all parsed data.
- `class MapLoader { static MapData Load(path, video); }` — parse-and-instantiate only, no GameSystem coupling.

**New `src/game/MapLoader.cpp`**:
- Parses top-level fields (`name`, `map_size`, `global_light`) then iterates the `objects` sequence.
- Dispatches by `type:` string to `ParseMesh`, `ParseLight`, `ParseCamera` helpers.
- **Mesh fallback**: if material load fails, creates a default `GameMaterial` from `MaterialDesc{}`. If mesh load fails, creates a procedural unit-cube via `renderer::CreateCubeMesh`.
- **Light type mapping**: `"global" → kGlobal`, `"omni" → kOmni`, `"circle_spot" → kCircleSpot`, `"rect_spot" → kRectSpot`.
- **Transform**: 16-float YAML sequence → `core::Mat4f(float*)` (row-major, matching Mat4f layout).
- **`editor:` block**: silently ignored (YAML-CPP skips unknown keys automatically).
- Per-object parse errors are caught and logged as `WARNING` — one bad object does not abort the whole map.

**Ref-counting for meshes**: `GetOrLoad` returns an AddRef'd template; the `GameMesh` constructor calls `AddRef()` again; `MapLoader` calls `tmpl->Release()` after construction. The fallback procedural material is released after passing to `MeshTemplate`.

### Ancillary changes

- `data/maps/.gitkeep` — establishes the map storage folder.
- `data/CLAUDE.md` — documented `maps/` and `.map.yaml` extension.
- `src/game/CMakeLists.txt` — added `MapLoader.cpp`.
- `src/game/CLAUDE.md` — added `GameObjectVisitor` and `MapLoader` to the module structure table.

---

## Decisions and rationale

**Visitor in headers**: All `Accept` overrides are one-liners; putting them in the header avoids three trivial `.cpp` files and keeps the pattern immediately visible at the class definition.

**MapData not added to GameSystem**: The issue explicitly requires the caller to decide what to do with the loaded objects. This separation lets the editor (future) and game (future) handle loading differently without MapLoader having runtime dependencies on GameSystem.

**Per-object exception handling**: A malformed YAML node for one object should not abort loading the rest of the map. Individual objects are wrapped in try/catch.

**Fallback material ID prefix `__default_`**: Consistent with the existing `__proc_` / `__mat_` convention in `MeshTemplate` for non-file-backed resources.

---

## Output to keep in mind

- `MapLoader::Load` returns owned `unique_ptr<GameObject>` objects that the caller must add to `GameSystem` (and eventually `GameSystem::RemoveObject` + `Release`).
- The `editor:` top-level YAML block is silently ignored — safe to round-trip through the serializer without breaking `MapLoader`.
- `GameObjectVisitor` is ready for the editor serializer (issue upcoming in the milestone).

---

## Skills and instructions used

- `impl-issue` skill
- `src/game/CLAUDE.md` — one class per `.h/.cpp`, structs as data holders may be header-only
- `src/CLAUDE.md` — Google C++ style, include root is `src/`
- `CLAUDE.md` — conventional commits, history file, cpplint before commit
