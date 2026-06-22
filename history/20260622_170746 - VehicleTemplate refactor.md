# VehicleTemplate — vehicle asset loading refactor (#734)

## What changed

Introduced `game/VehicleTemplate` as a reference-counted resource (following the
`MeshTemplate` / `ParticleSystemTemplate` pattern) and moved all vehicle descriptor
YAML parsing out of `GameVehicle` into it.

### Files added
- `src/game/VehicleTemplate.h` — new resource class declaration
- `src/game/VehicleTemplate.cpp` — YAML parsing, MeshTemplate loading, GetOrLoad/GetAll

### Files modified
- `src/game/GameVehicle.h/.cpp`
  - Removed `static Create(desc_path, video)` factory
  - Added `explicit GameVehicle(VehicleTemplate*)` public constructor (AddRef on entry,
    Release in destructor)
  - Removed private fields `desc_path_`, `vehicle_desc_`, `front_wheel_mesh_path_`,
    `rear_wheel_mesh_path_` — all now delegated to `VehicleTemplate`
  - `GetDescPath()` and `GetVehicleDesc()` delegate to `template_`
  - Removed `#include <yaml-cpp/yaml.h>`, `"core/Config.h"`, `"core/YamlSerialiser.h"`
- `src/game/MapLoader.cpp`
  - `ParseVehicle()` now calls `VehicleTemplate::GetOrLoad()` and constructs
    `std::make_unique<GameVehicle>(tmpl)` — consistent with `ParseMesh()` and
    `ParseParticleSystem()`
  - Added `#include "game/VehicleTemplate.h"`
- `src/game/CMakeLists.txt` — added `VehicleTemplate.cpp`

## Decisions and rationale

### Resource key: relative desc_path string
`VehicleTemplate` is keyed by the relative descriptor path (e.g.
`"vehicles/car.vehicle.yaml"`) rather than a full absolute path.  This matches
what the map YAML stores in the `desc` field and what `MapSerializer` emits from
`GetDescPath()`.  The full path is resolved only inside the constructor using
`core::Config::GetDataFolder()`.

### GetOrLoad returns nullptr on failure (unlike MeshTemplate)
`MeshTemplate::GetOrLoad` always returns a (possibly uninitialized) object.
`VehicleTemplate::GetOrLoad` returns `nullptr` when loading fails and calls
`Release()` to erase the partially-constructed entry from the registry, preventing
a broken cached entry from poisoning subsequent loads of the same path.  This is
the cleaner contract for a multi-step loader (YAML + 3 mesh files).

### Shared MeshTemplate references
`VehicleTemplate` holds AddRef'd `MeshTemplate` pointers for body, front wheel,
and rear wheel.  `GameVehicle` creates `GameMesh` children from these pointers
(each `GameMesh` constructor calls `AddRef` on the template it receives).
`VehicleTemplate` releases its refs in its destructor; the `GameMesh` objects
continue to hold the templates alive until they are destroyed.  Two vehicles in
the same map sharing the same `desc` path share one `VehicleTemplate` and
therefore the same `MeshTemplate` objects — no duplicate file I/O.

### BBox computed inline in constructor
The base `GameObject` constructor requires a `BBox3`.  With the old design this
was computed in `Create()` before calling `new GameVehicle(bbox)`.  The new
constructor uses an immediately-invoked lambda in the member-initialiser list so
the half_extents from the template are available before the base class is
constructed, without adding a static helper.

## Output to keep in mind

- `VehicleTemplate::GetAll()` is ready to be used by a future vehicle selection
  screen — no extra work needed on the template side.
- `MapSerializer::Visit(GameVehicle&)` calls `vehicle.GetDescPath().string()`;
  this still compiles because `GetDescPath()` now returns a `std::filesystem::path`
  constructed from the template's string ID.
- `GetVehicleDesc()` is still public on `GameVehicle` but only called internally
  (via `Activate()`). Consider making it private if no external caller ever needs it.

## Skills / instructions followed

- `src/game/CLAUDE.md` — one class per .h/.cpp pair, no yaml includes in game objects
- `src/CLAUDE.md` — Google C++ style, include root is `src/`
- Project CLAUDE.md — `cpplint` before commit, conventional commit message, history file
