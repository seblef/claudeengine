# Cache Jolt shapes per mesh template (#651)

## Problem

Every call to `PhysicsSystem::CreateBodyWithMesh` rebuilt a brand-new
`JPH::ConvexHullShape` or `JPH::MeshShape` from the raw vertex/index data,
even when all GameMesh instances shared the same `MeshTemplate`.  For a scene
with N instances of the same mesh, this wasted N–1 shape constructions and N–1
copies of the shape BVH in memory.

## Solution

A shape cache lives inside `PhysicsSystem` as:

```cpp
std::unordered_map<const void*, void*> mesh_shape_cache_;
```

The key is an opaque `const void*` (the `MeshTemplate*` pointer identity).
The value is a heap-allocated `JPH::ShapeRefC*`; the ref-counted pointer keeps
the Jolt shape alive for as long as the cache entry exists.  Using `void*` for
the value avoids exposing any Jolt type in `PhysicsSystem.h`.

`CreateBodyWithMesh` gains an optional trailing parameter:

```cpp
const void* shape_cache_key = nullptr
```

When non-null it:
1. Looks up the cache; if found, copies the `ShapeRefC` (bumps ref count) and
   skips shape construction entirely.
2. On a miss, builds the shape as before, then stores a `new JPH::ShapeRefC`
   in the cache so the next caller can reuse it.

`GameMesh::CreatePhysicsBody` passes `template_` as the cache key, so all
instances of the same `MeshTemplate` share one Jolt shape.

Cache cleanup happens at the top of `PhysicsSystem::~PhysicsSystem`, before
`bodies_.clear()`.  Each body already holds its own `AddRef()` on
`base_shape_`; releasing the cache entry just drops the extra ref that was
keeping the shape alive between calls.

## Ref-count walkthrough (N instances, all sharing one template)

| Event | Shape ref count |
|---|---|
| First `CreateBodyWithMesh` — shape built, cache entry created | 2 (local ShapeRefC + cache) → 2 after local goes out of scope: body AddRef + cache |
| Each subsequent call — cache hit, ShapeRefC copied | +1 per body (AddRef in PhysicsBody ctor) |
| Body destroyed — `base_shape_->Release()` | −1 per body |
| PhysicsSystem destroyed — cache deleted | −1 (last ref → 0 → shape freed) |

## Limitations / out of scope

- The cache key is the `MeshTemplate*` pointer; mixing ConvexHull and Exact
  shape types for the same template pointer would return the wrong shape type.
  Per issue #651 this is out of scope (a given template should use a single
  shape type).
- The cache is never pruned during a session.  Templates are ref-counted and
  long-lived per level, so this is acceptable overhead.

## Files changed

- `src/physics/PhysicsSystem.h` — added `#include <unordered_map>`,
  `shape_cache_key` param on `CreateBodyWithMesh`, `mesh_shape_cache_` field.
- `src/physics/PhysicsSystem.cpp` — cache lookup/store in
  `CreateBodyWithMesh`; cache cleanup in destructor.
- `src/game/GameMesh.cpp` — pass `template_` as `shape_cache_key`.

## Skills and instructions used

- `impl-issue` skill (instructions in `/.claude/` tools)
- `src/CLAUDE.md`, `src/physics/CLAUDE.md`, `src/game/CLAUDE.md`
- Google C++ style guide (one class per file, no Jolt types in public headers)
