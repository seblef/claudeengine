# Tire Track System

## Overview

Implements per-wheel tire track ribbons rendered as alpha-blended ground decals.
Tracks support three surface types (terrain, road, generic), fade over time, and
have a fixed GPU budget per wheel.

## Changes

### `src/physics/SurfaceMaterial.h` (new)
- `SurfaceType` enum: `kGeneric`, `kTerrain`, `kRoad` with `kSurfaceTypeCount = 3`.

### `src/physics/PhysicsSystem.h/.cpp`
- Replaced `std::unordered_set<uint32_t> terrain_body_ids_` with
  `std::unordered_map<uint32_t, SurfaceType> surface_types_` (merging both maps).
- `CreateTerrainBody` auto-registers with `kTerrain`.
- `DestroyBody` erases from `surface_types_`.
- Added `SetBodySurfaceType(body, type)` for road/custom geometry tagging.
- Added `GetSurfaceType(jolt_body_id)` for runtime surface queries.
- Updated `BodyDebugFilter` to check the map value instead of the old set.

### `src/physics/PhysicsVehicle.h/.cpp`
- Added `WheelContact` struct: `has_contact`, `position`, `contact_body_id`.
- Added `GetWheelContact(int wheel_index)` — reads from `JPH::Wheel::HasContact()`,
  `GetContactPosition()`, `GetContactBodyID()` after each `PhysicsSystem::Step()`.
- Added `GetWheelCount()` — wraps `constraint_->GetWheels().size()`.

### `src/core/VertexTrack.h` (new)
- 32-byte vertex: `position(vec3) | uv(vec2) | alpha(float) | _pad(float[2])`.

### `src/core/VertexType.h`
- Added `kTrack` to the `VertexType` enum and corresponding entry in `kVertexSize`.

### `src/gldevices/GLVertexBuffer.cpp`
- Added `kTrack` case in `ConfigureAttributes`:
  location 0 = position (vec3), location 1 = uv (vec2), location 2 = alpha (float).

### `src/track/TireTrackSystem.h/.cpp` (new)
- Flat `TrackQuad` struct at namespace level (accessible by file-local helpers).
- Per-wheel ring buffer (`std::array<TrackQuad, 512>`), head/count for wrap-around.
- One VBO per wheel (pre-allocated for 512 × 4 vertices, `kDynamic`).
- One shared static IBO: triangle pairs `[0,1,2, 2,1,3]` for all 512 quad slots.
- `Update(dt)`: ages quads, prunes dead ones, emits new quads on contact.
- `Render(camera)`: rebuilds vertex scratch buffer sorted by surface type,
  uploads with `Fill()`, issues up to 3 draw calls per wheel per frame.
- `RegisterVehicle` / `UnregisterVehicle` for dynamic vehicle lifecycle.
- Texture array indexed by `SurfaceType` integer.

### `src/track/CMakeLists.txt`
- Added `TireTrackSystem.cpp` source.
- Added `abstract` and `physics` as public dependencies.

### `src/renderer/Renderer.h/.cpp`
- Added `SetTireTrackSystem` / `GetTireTrackSystem` following `SetParticleRenderer` pattern.
- Forward-declared `track::TireTrackSystem` in the header.
- New pass **4c** (tire tracks) — renders before particles (4d) so tracks appear under
  particle effects on the ground.  Sets `SrcAlpha` blend, depth test on, depth write off,
  `kLessEqual` depth func.

### `src/renderer/CMakeLists.txt`
- Added `track` to `target_link_libraries`.

### `src/game/GameSystem.h/.cpp`
- Owns `std::unique_ptr<track::TireTrackSystem> tire_track_system_`.
- Constructed in the `GameSystem` constructor and registered with `Renderer`.
- `Update()` calls `tire_track_system_->Update(dt)` immediately after `PhysicsSystem::Step()`.

### `src/game/GameVehicle.cpp`
- `Activate()` calls `TireTrackSystem::RegisterVehicle(physics_vehicle_)` via Renderer.
- `Deactivate()` calls `TireTrackSystem::UnregisterVehicle(physics_vehicle_)` before
  handing the vehicle back to `PhysicsSystem`.

### `data/shaders/glsl/forward/tire_track_vs.glsl` (new)
- Takes `VertexTrack` attributes (location 0/1/2).
- Applies world-up Z-fight bias (`+0.005 m` Y) before MVP transform.
- Passes `v_uv` and `v_alpha` to the fragment stage.

### `data/shaders/glsl/forward/tire_track_ps.glsl` (new)
- Samples `track_tex` at binding 0.
- Outputs `vec4(albedo.rgb, albedo.a * v_alpha)`.
- No lighting — flat decal.

### `data/textures/tracks/` (new)
- `track_terrain.png`, `track_road.png`, `track_generic.png` — 64×64 grayscale
  placeholder textures with simple tread patterns.

## Key Decisions

### Surface type as a map on PhysicsSystem, not on PhysicsBody
Querying via `uint32_t` Jolt body ID allows `TireTrackSystem` to resolve surface
types without holding a `PhysicsBody*` reference across frames.

### `TireTrackSystem` at game/world level, registered with Renderer
Following the `ParticleRenderer` precedent.  GameVehicle accesses it via
`Renderer::GetTireTrackSystem()` to avoid a direct dependency on `GameSystem`.

### VBO rebuild every frame, sorted by surface type
Each `Render()` call collects live quads into a scratch buffer ordered
[generic | terrain | road] and uploads with one `Fill()` call per wheel.
This enables up to 3 `RenderIndexed` draw calls per wheel using the static IBO
with index offsets, avoiding dynamic IBO updates entirely.

### Quad half-length tied to emit_interval
`half_length = emit_interval / 2` ensures back-to-back quads tile seamlessly
with no gap and no overlap.

### Z-fight offset in the vertex shader via world-up
The issue spec recommends pushing along the contact normal.  World-up is a safe
approximation for terrain and road surfaces; storing the normal per-quad would
add 12 bytes per quad (3 × float).

## Notes for Future Features

- Road bodies need `PhysicsSystem::SetBodySurfaceType(body, SurfaceType::kRoad)`
  called by the map serializer after creating mesh bodies for road geometry.
- Texture paths can be loaded by the game app: call
  `TireTrackSystem::SetTrackTexture(SurfaceType::kTerrain, video->CreateTexture("tracks/track_terrain.png"))`.
- Skid mark intensity from `Wheel::GetContactLongitudinal()` is out of scope but
  the `alpha` field could be modulated per-quad at emission time.

## Skills and Instructions Used
- `impl-issue` skill
- `src/CLAUDE.md`: one class per `.h/.cpp` pair, Google C++ style
- `src/physics/CLAUDE.md`: Jolt types must not appear in public physics headers
- `data/shaders/glsl/CLAUDE.md`: `_vs.glsl` / `_ps.glsl` naming, `#include` for uniforms
