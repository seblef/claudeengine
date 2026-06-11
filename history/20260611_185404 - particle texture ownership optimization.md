# Particle Texture Ownership Optimization

**Issue:** #463  
**Branch:** feat/particle-texture-ownership-463

## Summary

Particle textures were being loaded and released every frame inside
`ParticleRenderer::RenderGeometryPass` and `ParticleRenderer::RenderForwardPass`,
causing redundant GPU resource lookups on each frame.

## Changes

### `ParticleEmitter` (`particles/ParticleEmitter.h`, `ParticleEmitter.cpp`)

- Added `abstract::Texture* texture_` member (initialized to `nullptr`).
- Constructor now calls `video->CreateTexture(desc_.texture)` once (guarded by
  an empty-string check) and stores the result.
- Added explicit destructor that calls `texture_->Release()` if non-null.
- Deleted copy and move constructors/assignment operators (the emitter owns a
  ref-counted GPU resource and must not be copied or moved implicitly).
- Added `GetTexture() const` getter used by `ParticleRenderer`.

### `ParticleRenderer` (`particles/ParticleRenderer.cpp`)

- Replaced `video_->CreateTexture(desc.texture)` / `tex->Release()` calls in
  both `RenderGeometryPass` and `RenderForwardPass` with `emitter->GetTexture()`.
  No lifetime management needed in the renderer — the emitter owns the texture.

### `GameParticleSystem` (`game/GameParticleSystem.h`, `GameParticleSystem.cpp`)

- Changed `std::vector<particles::ParticleEmitter>` to
  `std::vector<std::unique_ptr<particles::ParticleEmitter>>` because `ParticleEmitter`
  is now non-movable. The lambda in `std::transform` was replaced with a simple
  range-for + `emplace_back(make_unique<...>)`.
- Updated all iteration sites (`Register`, `Unregister`, `SetWorldTransform`,
  `Update`) to dereference the `unique_ptr` via `->` / `.get()`.

## Decisions

- **Empty texture guard:** `desc_.texture.empty()` is checked before calling
  `CreateTexture` to avoid passing an empty string to the backend, keeping
  `texture_` null in that case. The renderer already handled null gracefully
  with `if (!tex) continue`.
- **Non-movable emitter:** Deleting move semantics is the correct choice here —
  the emitter owns a GPU resource with a ref-counted lifetime. Allowing
  accidental moves would risk double-Release bugs.
- **`unique_ptr` storage in `GameParticleSystem`:** This is the natural
  consequence of a non-movable type. It also makes the pointer stability
  guarantee explicit (emitter addresses are stable after `reserve`, which
  `ParticleRenderer` relies on for its raw-pointer list).

## Output to keep in mind

- `ParticleEmitter` is now non-copyable and non-movable. Any new code that needs
  to store or transfer emitters must use `unique_ptr`.
- `GetTexture()` returns `nullptr` for emitters with no texture; callers must
  handle that case.

## Skills and instructions used

- `impl-issue` skill flow (checkout dev, branch, implement, cpplint, commit, PR).
- `src/CLAUDE.md`: one class per `.h`/`.cpp`, Google C++ style guide, include
  root is `src/`.
- `src/particles/CLAUDE.md`: dependency graph — `particles` sits below `renderer`.
