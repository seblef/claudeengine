# CharacterController — Jolt CharacterVirtual wrapper

**Date:** 2026-06-17
**Issue:** #564
**Branch:** feat/character-controller-564

---

## What was implemented

Two new files:

- `src/physics/CharacterController.h` — public API, Jolt-free
- `src/physics/CharacterController.cpp` — Jolt implementation

And changes to existing files:

- `src/physics/PhysicsSystem.h` — added `CharacterController` forward-declaration and `CreateCharacter()` factory
- `src/physics/PhysicsSystem.cpp` — added `CreateCharacter()` implementation and `#include "physics/CharacterController.h"`
- `src/physics/CMakeLists.txt` — added `CharacterController.cpp` to the static library

---

## API summary

```cpp
// Factory on PhysicsSystem:
std::unique_ptr<CharacterController> CreateCharacter(
    float capsule_radius,
    float capsule_half_height,
    const core::Mat4f& initial_transform);

// On CharacterController:
void        Move(const core::Vec3f& velocity, float dt);
core::Mat4f GetWorldTransform() const;
bool        IsGrounded() const;
```

`CharacterController` is self-owning: the caller resets the `unique_ptr` to destroy it.
No `DestroyCharacter()` is needed.

---

## Decisions and rationale

### Capsule shape offset

Jolt's `CapsuleShape(half_height, radius)` is centered at the midpoint of the cylinder portion.
To place the capsule base at the character's world position, the shape is wrapped in a
`RotatedTranslatedShape` that offsets it upward by `half_height + radius` (half-cylinder + bottom hemisphere).

The issue spec says "offset by `half_height`", which is slightly imprecise; the Jolt samples
(`CharacterVirtualTest.cpp`) use `half_height + radius` — that is the value used here.

### Supporting volume

`settings.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -radius)` is set so that
only the lower hemisphere of the capsule accepts ground contacts, matching Jolt best-practice
and preventing the character from "sticking" to walls.

### IsGrounded maps to IsSupported()

`CharacterBase::IsSupported()` returns true for both `OnGround` and `OnSteepGround` states.
This satisfies the issue spec ("True when the character is standing on a surface"), while
giving game code the option to query `character_->GetGroundState()` if finer-grained logic
is ever needed (not exposed today).

### Explicit destructor to avoid incomplete-type deletion

`std::unique_ptr<JPH::CharacterVirtual>` requires a complete type at the point of deletion.
Because `CharacterController.h` only forward-declares `JPH::CharacterVirtual`, the destructor
is declared in the header and defined (`= default`) in the `.cpp` where the full Jolt header
is included. This is the standard pimpl/incomplete-type idiom.

### PhysicsSystem does not track CharacterController instances

`PhysicsBody` instances are owned by `PhysicsSystem`; they are destroyed when the system shuts down.
`CharacterController` is owned by the caller so it can be released independently at any time.
`JPH::CharacterVirtual` without an inner body has no registered state inside the Jolt world, so
no deregistration is needed on destruction — the destructor just frees memory.

### Collision layer

`kLayerPlayer` is used for both the broad-phase and object-layer filters passed to
`CharacterVirtual::Update()`, consistent with the collision matrix defined in `PhysicsSystem.cpp`
(player layer collides with all other layers).

### gravity in Move()

`CharacterVirtual::Update()` takes a gravity vector used only to push the character down onto
objects it stands on (applies a small downward force proportional to mass).  The caller's
`velocity` argument must already incorporate gravity for proper free-fall — this matches Jolt's
design ("it's your own responsibility to apply gravity to the character velocity").
`jolt_system_->GetGravity()` is forwarded so the weight-on-objects behavior is consistent with
the simulation's global gravity.

---

## Files from skills / CLAUDE.md taken into account

- `src/physics/CLAUDE.md` — no Jolt types in public headers; one class per file pair; forward-declare Jolt types in `.h`, include in `.cpp`
- `src/CLAUDE.md` — Google C++ style, one class per `.h/.cpp`, include root is `src/`
- Root `CLAUDE.md` — cpplint must pass; history file required; PR to `dev`

---

## Output to keep in mind for next features

- `CharacterController::Move()` does not do stair-stepping or stick-to-floor (`ExtendedUpdate`);
  if those are needed later, expose them as separate methods or a settings struct.
- `IsGrounded()` does not distinguish flat vs. steep ground; add `GetGroundState()` forwarding
  if the game needs slope detection.
- `CharacterController` instances are not tracked by `PhysicsSystem`; if a query over all
  active characters is ever needed, a registration mechanism must be added.
