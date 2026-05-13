# GameLight and GameLightDesc

**Date:** 2026-05-12
**Issue:** #126
**PR:** #135
**Branch:** feat/game-light

## Changes

- `src/game/GameLightDesc.h` — header-only struct with defaults for all four light types
- `src/game/GameLight.h` — class declaration
- `src/game/GameLight.cpp` — implementation including `CreateRendererLight` factory
- `src/game/CMakeLists.txt` — added `GameLight.cpp`

## Decisions

**Single class, no subclass hierarchy:** All four renderer light types are handled via a `switch` in `CreateRendererLight()`. Unused descriptor fields (e.g. `radius` for a GlobalLight) are silently ignored. This avoids a parallel game-layer light hierarchy at the cost of slightly looser field scoping — acceptable for this milestone.

**Light created in constructor, not OnAddedToScene:** The renderer light is constructed immediately and stored in `unique_ptr<renderer::Light>`. This ensures `GetLight()` is always valid and `OnWorldTransformUpdated()` never needs a null check.

**GlobalLight world matrix:** `GlobalLight` constructor does not take a world matrix (it is always-visible with infinite influence). `OmniLight`, `CircleSpotLight`, and `RectangleSpotLight` receive `Mat4f::kIdentity` at construction; their world matrix is updated on the first `SetWorldTransform()` call.

**GameObject BBox3{}:** The game-layer local bbox is zero-sized at origin. The renderer::Light manages its own spatial data (through Renderable) and is responsible for visibility; the game-layer bbox on GameObject is unused for lights in this milestone.

## Output to keep in mind

- `GameLightDesc` is header-only (no .cpp) per the game CLAUDE.md convention for data-holder structs.
- The `cppcheck-suppress unusedStructMember` comment on `light_` was needed since cppcheck cannot see the usage through `unique_ptr`.
- `cpplint` requires at least two spaces between code and trailing comments — enforced in `GameLightDesc.h`.

## Skills / instructions used

- `src/CLAUDE.md`: one class per `.h`/`.cpp` pair; structs used purely as data holders may be header-only
- `src/game/CLAUDE.md`: structs used purely as data holders may be header-only
- Root `CLAUDE.md`: git workflow, conventional commits
