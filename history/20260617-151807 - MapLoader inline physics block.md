# MapLoader: parse inline `physics:` block (issue #566)

## What changed

`src/game/MapLoader.cpp` — anonymous namespace gains `ParsePhysicsBodyDesc`.

### `ParsePhysicsBodyDesc(const YAML::Node&) → std::optional<physics::PhysicsBodyDesc>`

Reads the optional `physics:` sub-node that can appear on any `type: mesh` object
in a `.map.yaml` file. Returns `std::nullopt` when the node is absent, so meshes
without a physics block are unaffected.

**Shape parsing** (`shape:` key):
| YAML value | `PhysicsShapeType` | Extra keys consumed |
|---|---|---|
| `box` (default) | `Box` | `half_extents: [x, y, z]` |
| `sphere` | `Sphere` | `radius` |
| `cylinder` | `Cylinder` | `radius`, `half_height` |
| `capsule` | `Capsule` | `radius`, `half_height` |
| `convex_hull` | `ConvexHull` | — |
| `exact` | `Exact` | — |
| unknown | falls back to `Box(0.5,0.5,0.5)` + warning log |

**Motion type** (`motion_type:` key, default `static`):
`static` → `MotionType::Static`, `kinematic` → `Kinematic`, `dynamic` → `Dynamic`.

**Static enforcement**: if `shape` is `exact` or `terrain` and `motion_type` is not
`static`, a `LOG_F(WARNING, …)` is emitted and `MotionType::Static` is forced.

**Material fields** read with per-field defaults from `PhysicsMaterialDesc`:
`friction`, `restitution`, `mass`, `linear_damping`, `angular_damping`,
`gravity_factor`.

**Collision fields**: `collision_layer` (default `kLayerWorld = 0`),
`collision_mask` (default `0xFFFF`).

### `ParseMesh` update

After constructing the `GameMesh` and before `SetWorldTransform`:

```cpp
if (auto desc = ParsePhysicsBodyDesc(node["physics"]))
    mesh->SetPhysicsDesc(*desc);
```

## Decisions

- **Free function in anonymous namespace**: consistent with all other parse helpers in
  `MapLoader.cpp`; keeps the function private to the translation unit.
- **`std::optional` return**: the caller can test presence cheaply with an `if`; avoids
  sentinel/null-pointer patterns.
- **No new files**: the feature is entirely within `MapLoader.cpp`; no new class or
  header is needed.
- **Direct `#include` of physics headers**: Google style requires each file to include
  the headers it directly uses, even if they are transitively available through
  `GameMesh.h`.

## Keep in mind for future features

- `exact` and `terrain` shape types carry no geometry in `PhysicsShapeDesc`; the
  geometry is supplied at body-creation time via `MeshData` / `TerrainData`.  Any
  loader or builder touching those shapes must ensure the template's CPU-side mesh data
  is still live at that point (see `MeshTemplate::GetMeshData()`).
- `convex_hull` is similar — no shape parameters, geometry from `MeshData`.
- `collision_layer` and `collision_mask` are `uint16_t`; YAML integers larger than
  65535 are silently truncated by the `static_cast`.

## Skills / CLAUDE.md instructions observed

- `impl-issue` skill workflow followed (branch from `dev`, conventional commit, PR to `dev`).
- `src/CLAUDE.md`: one class per file, Google style, include root is `src/`.
- `src/game/CLAUDE.md`: `MapLoader` is a static utility; objects returned are not added
  to `GameSystem` — that is the caller's responsibility.
- `src/physics/CLAUDE.md`: Jolt types must not appear in public headers; only
  `physics/*.h` types used here.
