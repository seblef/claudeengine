# PhysicsSystem — World Initialization and Simulation Step

**Issue:** #562  
**Branch:** `feat/physics-system-562`  
**Skills used:** `impl-issue`

## Changes

### New files

- `src/physics/PhysicsBody.h` — Minimal opaque handle for a body registered in the physics world. Stores a Jolt body ID as `uint32_t` (Jolt-free public API), motion type, and listener pointer. `PhysicsSystem` is declared a `friend`. Intended to be expanded when body creation is implemented (downstream issue).
- `src/physics/PhysicsSystem.h` — Singleton owning the Jolt world (`JPH::PhysicsSystem`), temp allocator, and thread-pool job system. Public API is Jolt-free; Jolt forward-declarations keep headers clean.
- `src/physics/PhysicsSystem.cpp` — Full implementation including collision layer setup, Jolt init/shutdown, and per-frame simulation step.

### Modified files

- `CMakeLists.txt` — Added `SOURCE_SUBDIR "Build"` to the JoltPhysics FetchContent declaration. Jolt v5.5.0 has no root `CMakeLists.txt`; the build entry point is `Build/CMakeLists.txt`. Without this fix the `Jolt` CMake target was never created, and Jolt headers were not on the include path.
- `src/physics/CMakeLists.txt` — Added `PhysicsSystem.cpp` to the library. Removed the broken `target_include_directories(physics PRIVATE "${JoltPhysics_SOURCE_DIR}")` line (the variable name was wrong — FetchContent uses lowercase `joltphysics_SOURCE_DIR`). After the `SOURCE_SUBDIR` fix, the Jolt target now exports its root directory as a `PUBLIC` include via `$<BUILD_INTERFACE:${PHYSICS_REPO_ROOT}>`, so no explicit include line is needed.
- `src/game/GameSystem.cpp` — Calls `PhysicsSystem::Instance().Step(dt)` before `Renderer::Update()` (guarded by `IsInstanced()` so the engine still runs without a physics world).
- `src/game/CMakeLists.txt` — Added `physics` to `game`'s `PUBLIC` link libraries.

## Key decisions

### PhysicsBody created here, not in a downstream issue

`RaycastResult.h` had a comment stating "full type defined in physics/PhysicsBody.h (downstream issue)". However, `PhysicsSystem::Step()` must iterate registered `PhysicsBody` instances, so `PhysicsBody` is a hard dependency of this issue. A minimal class was added now; a future issue will add the body creation API (`PhysicsSystem::CreateBody()`).

### LayerFilters allocated on the heap

Jolt's `BroadPhaseLayerInterfaceTable`, `ObjectLayerPairFilterTable`, and `ObjectVsBroadPhaseLayerFilterTable` derive from `NonCopyable` and have no default constructors. Storing them directly as struct members would require initialising all three in the constructor initialiser list, but `ObjectVsBroadPhaseLayerFilterTable` must be built *after* the other two are fully configured. Using `std::unique_ptr<>` for each member allows two-phase construction inside the constructor body.

### IsInstanced() guard in GameSystem::Update()

`GameSystem` does not own `PhysicsSystem` (per the issue spec). The guard ensures an app that does not create a `PhysicsSystem` (e.g., a pure rendering demo) still compiles and runs without change.

### JPH::Factory::sInstance instead of Create/DestroyInstance()

Jolt v5.5.0's `Factory` class exposes only a static `sInstance` pointer — no `CreateInstance()`/`DestroyInstance()` helper methods exist. Init sets `JPH::Factory::sInstance = new JPH::Factory()` and the destructor does `delete JPH::Factory::sInstance; JPH::Factory::sInstance = nullptr`.

## Output to keep in mind

- `PhysicsBody::jolt_id_` stores the raw `uint32_t` returned by `JPH::BodyID::GetIndexAndSequenceNumber()`. When creating bodies (downstream), store the value from the Jolt `BodyID` returned by `BodyInterface::CreateAndAddBody()`.
- `PhysicsSystem::RegisterBody` / `UnregisterBody` are the only registration hooks — the transform dispatch in `Step()` iterates the `bodies_` vector. Body creation + registration will be the next physics milestone.
- The `LayerFilters` static local in `Init()` is constructed once on first call and lives for the remainder of the process. If `Init()` is called again on a new `PhysicsSystem` instance, the same filter objects are reused (they are stateless after construction).
