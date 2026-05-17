# MapSerializer — Save and Load (Issue #252)

## Summary

Implemented `editor::MapSerializer`, a static utility that serialises an
`EditorScene` (plus editor camera state) to a `.map.yaml` file and
deserialises one back into a live `EditorScene`.

## New files

| File | Role |
|---|---|
| `src/editor/MapSerializer.h` | Public API: `Save()`, `Load()`, `MapLoadResult`, inner `SerializeVisitor` |
| `src/editor/MapSerializer.cpp` | Full implementation |

`src/editor/CMakeLists.txt` updated to compile `MapSerializer.cpp`.

---

## Design decisions

### Visitor-based serialisation

`Save()` iterates `scene.GetObjects()` and calls `obj->Accept(visitor)` for
every **dynamic** object (non-dynamic objects — the floor and the global
directional light — are handled separately: the floor is skipped entirely, the
global light is written under the top-level `global_light:` key via
`GetGlobalLightDesc()`).

Inside the visitor, `Visit(GameMesh&)` additionally skips any mesh whose
`MeshTemplate` id starts with `"__proc_"` (covers the default procedural
cube that is always present in a new scene). Only file-backed meshes with
real paths are emitted.

### Light type dispatch

`Visit(GameLight&)` uses a `switch` on `renderer::LightType` to cast the
`renderer::Light*` to its concrete subclass and emit type-specific fields
(radius for omni, direction + angles + range for spot types, direction +
ambient for global). All light types share the common fields (color,
intensity, cast_shadow, shadow_resolution, shadow_bias).

### Load — resource ownership

`MapLoader::Load()` returns `MapData` with fully constructed `GameObject`
instances. Each `GameMesh` in `MapData` holds one AddRef on its
`MeshTemplate` (and the template holds one AddRef on its `GameMaterial`).

`MapSerializer::Load()` must ensure `EditorScene` can properly release
templates and materials when it is destroyed. The approach:
- For each new `GameMesh` object, before handing it off via
  `AddDynamicObject`, call `tmpl->AddRef()` and `mat->AddRef()` then
  register them with `scene->AddMeshTemplate()` / `scene->AddGameMaterial()`.
- `EditorScene::~EditorScene()` calls `Release()` on every stored template
  and material, then destroys `dynamic_objects_` (each `GameMesh` destructor
  calls `tmpl->Release()` again) — the two Release calls balance the two
  AddRef calls.
- Duplicate detection via `std::unordered_set<std::string>` keyed by
  `GetId()` prevents double-registration when multiple meshes share the same
  template or material.

### Editor state round-trip

The `editor:` YAML block stores the camera orbit parameters
(`focus`, `azimuth`, `elevation`, `distance`) and the name of the selected
object (`null` when nothing is selected). On load, the selected object is
looked up by name in `scene->GetObjects()`.

`snap_grid` is always written as `0.0` (not yet implemented).

---

## Output format (reference)

```yaml
name: MyMap
map_size: 120.0

global_light:
  direction: [-0.4, -0.8, -0.3]
  color: [0.9, 0.85, 0.7]
  intensity: 1.2
  ambient_color: [0.0, 0.0, 0.0]
  cast_shadow: true
  shadow_resolution: 1024
  shadow_bias: 0.005

objects:
  - name: Cube
    type: mesh
    transform: [1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1]
    mesh: meshes/cube.obj
    material: materials/default.yaml

editor:
  selected_object: null
  camera_focus: [0.0, 0.0, 0.0]
  camera_azimuth: 0.52
  camera_elevation: 0.3
  camera_distance: 15.0
  snap_grid: 0.0
```

---

## Things to keep in mind for next features

- **Save/Load wiring**: `MapSerializer` is a standalone utility; it still
  needs to be wired into `EditorSystem` (File → Save / File → Open menu
  actions or keyboard shortcuts).
- **Default cube on load**: the `EditorScene` constructor (second form) always
  adds a default procedural cube as the first dynamic object. Loaded maps
  therefore always start with this cube in addition to the saved content.
  If this becomes undesirable, the second constructor should be changed to
  omit the default cube, or a flag should be added to suppress it.
- **`snap_grid`**: currently always written as `0.0` and ignored on read.
  Future implementation should read it back and wire it into an editor grid
  snap setting.
- **Material path convention**: materials are saved as
  `materials/{name}.yaml`. The `StripMaterialPath` helper in `MapLoader`
  already strips this prefix, so the round-trip is consistent.

---

## Skills and instructions followed

- `src/CLAUDE.md`: one class per `.h`/`.cpp` pair; Google C++ style;
  include root is `src/`.
- `src/editor/CLAUDE.md`: editor is the leaf of the dependency graph
  (no game/renderer headers are included from `.h`); one class per file pair.
- `impl-issue` skill workflow: checkout `dev`, pull, create feature branch,
  implement, cpplint, commit, open PR to `dev`.
