# JPH_DEBUG_RENDERER enabled for all builds

**Issue:** #622  
**PR:** #629  
**Branch:** `feat/jolt-debug-renderer-622`

## Change

Single line added to `CMakeLists.txt`, immediately after `FetchContent_MakeAvailable(JoltPhysics)`:

```cmake
target_compile_definitions(Jolt PUBLIC JPH_DEBUG_RENDERER)
```

## Decisions

**Why `PUBLIC` on the `Jolt` target (not `PRIVATE` on `physics`)?**  
`JPH_DEBUG_RENDERER` must be defined consistently in every translation unit that includes Jolt headers. Marking it `PUBLIC` on the `Jolt` target propagates it transitively to all consumers (`physics`, the editor), so no per-module additions are needed and there is no risk of ODR violations from mismatched defines.

**Why at the root `CMakeLists.txt` level (not inside `src/physics/CMakeLists.txt`)?**  
`Jolt` is fetched and its CMake target is created at the root level. `src/physics` only links against it. Modifying the `Jolt` target's properties from `src/physics` would work today but feels backwards — the root is the natural owner.

**Global (dev + stable) rather than dev-only?**  
The issue explicitly requires both dev and stable so the stable editor can visualize physics shapes. No `if(BUILD_TYPE STREQUAL dev)` guard was added.

## Verification

Both configurations compiled without errors or new warnings:
- `cmake -B _build_dev  -DCMAKE_BUILD_TYPE=Debug   -DBUILD_EDITOR=ON`
- `cmake -B _build_stable -DCMAKE_BUILD_TYPE=Release -DBUILD_EDITOR=ON`

## Skills / instructions used

- `impl-issue` skill (workflow: checkout dev, branch, implement, commit, PR)
- `CLAUDE.md`: conventional commits, history file requirement
