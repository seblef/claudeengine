# Physics Shape Disk Cache

**Issue:** #778  
**Branch:** `feat/physics-shape-disk-cache`  
**Date:** 2026-06-24

---

## Summary

Adds a transparent L2 disk cache for Jolt `HeightFieldShape` (terrain) and `MeshShape` / `ConvexHullShape` (static meshes). Jolt builds a BVH acceleration structure at shape-creation time, which is expensive on large maps. Serialising the fully-built shape means subsequent loads are near-zero-cost deserialisations.

The existing in-memory `mesh_shape_cache_` (keyed on `shape_cache_key` pointer) is kept as the L1 session cache. The new disk cache is the L2 layer that survives restarts.

---

## Design decisions

### Cache key strategy — FNV-1a 64-bit hash

xxHash was not available as a project dependency (Jolt does not bundle it). FNV-1a 64-bit was chosen as the hash function. It is parameter-free, requires no external dependency, and is fast for multi-kilobyte payloads. The hash is computed over:
- **Terrain:** the raw `uint16_t` heightmap sample array.
- **Meshes:** the flat `float` vertex array hashed first, then the `uint32_t` index array combined into the same accumulator via FNV-1a continuation.

The Jolt version (major.minor) is baked into the filename (`<hash>_jolt5.5.bin`) so a Jolt version bump automatically invalidates all cached entries without any manual intervention.

### Serialisation API — `SaveWithChildren` / `sRestoreWithChildren`

The issue mentioned `ObjectStreamBinaryIn` / `ObjectStreamBinaryOut`, but Jolt's `Shape::SaveWithChildren` and `Shape::sRestoreWithChildren` are the correct modern API for serialising a fully-built shape (BVH included). `ObjectStream` serialises `ShapeSettings`, not built shapes. `StreamOutWrapper` / `StreamInWrapper` bridge Jolt's abstract stream interface to `std::ofstream` / `std::ifstream`.

### Stale-file cleanup — terrain only

The issue requested "one `.bin` file per terrain — replace on change" and "one `.bin` per mesh template — replace on change". Since the cache is content-addressed, mesh files cannot accumulate (each unique mesh geometry gets exactly one file). Terrain files CAN accumulate if terrain data changes: the old `<old_hash>_jolt*.bin` remains alongside the new one.

The `SaveShape` helper accepts a `cleanup_siblings` flag: when `true`, all other `*.bin` files in the same directory are deleted before writing. This flag is set for terrain (one terrain per dir) and unset for meshes (multiple valid concurrent entries). There is no active mesh-file cleanup — the directory contains exactly as many `.bin` files as there are unique mesh templates, which is not accumulation.

### Directory layout

```
<cache_dir>/
  terrain/<hash>_jolt5.5.bin     # one file per terrain; replaced when terrain changes
  meshes/<hash>_jolt5.5.bin      # one file per unique mesh; grow with the asset set
```

Using separate subdirectories keeps terrain vs. mesh cleanup scoped correctly and keeps the root cache dir clean.

### Private internal header

`PhysicsShapeCache.h` is a private header — only `#include`'d by `PhysicsSystem.cpp`. It is not installed or exported and never included transitively by `game/` or `editor/`. Jolt types appear in it but do not escape the public physics API.

### Configuration

`physics.shape_cache_dir` in `data/config.yaml` (default: `_build/physics_cache`). Parsed by the new `core::PhysicsConfig` class (following the same pattern as `ShadowConfig`, `PlayerConfig`, etc.). If the key is absent or the value is empty, disk caching is disabled and behaviour is identical to before.

---

## Files changed

| File | Change |
|---|---|
| `src/physics/PhysicsShapeCache.h` | New private header: `HashBytes`, `ShapeCachePath`, `TryLoadShape`, `SaveShape` |
| `src/physics/PhysicsShapeCache.cpp` | New: implements the four helpers |
| `src/physics/PhysicsSystem.h` | Added `SetShapeCacheDir()`, `shape_cache_dir_`, `<string_view>` include |
| `src/physics/PhysicsSystem.cpp` | Wired cache into `CreateTerrainBody` and `CreateBodyWithMesh`; include `PhysicsShapeCache.h` |
| `src/physics/CMakeLists.txt` | Added `PhysicsShapeCache.cpp` |
| `src/core/PhysicsConfig.h` | New: parses `physics.shape_cache_dir` from YAML |
| `src/core/PhysicsConfig.cpp` | New: implements `PhysicsConfig::Parse` |
| `src/core/AppConfig.h` | Added `PhysicsConfig` member + `GetPhysics()` accessor |
| `src/core/AppConfig.cpp` | Added `physics_` static + parsing |
| `src/core/CMakeLists.txt` | Added `PhysicsConfig.cpp` |
| `data/config.yaml` | Added `physics.shape_cache_dir: _build/physics_cache` |
| `src/app/main.cpp` | Calls `SetShapeCacheDir` after `Init()` |

---

## Skills and CLAUDE.md rules applied

- `src/CLAUDE.md`: one class per `.h`/`.cpp` pair; utility functions in a `*Utils.cpp` file — `PhysicsShapeCache` holds only free functions (no class), which is consistent with the "utility" spirit.
- `src/physics/CLAUDE.md`: Jolt types must not appear in public `physics/*.h`. `PhysicsShapeCache.h` is private and never included by consumers.
- `src/CLAUDE.md`: `#include` root is `src/`.
- Google C++ Style Guide — enforced by `cpplint`.
- Conventional commits for the commit message.

---

## Output / notes for next contributions

- `PhysicsShapeCache.h` is a private header. Do not add it to any target's public include path.
- The cache directory is created lazily on first write; no need for users to pre-create it.
- The Jolt binary format is not backwards-compatible across major Jolt versions. The filename encodes `jolt<MAJOR>.<MINOR>` to auto-invalidate entries on version bumps. No patch-level invalidation is done (patch releases are assumed to be binary-compatible).
- Mesh cleanup is deliberately omitted: the mesh cache grows proportionally to the number of unique mesh templates in the scene, which is bounded and not a maintenance problem.
