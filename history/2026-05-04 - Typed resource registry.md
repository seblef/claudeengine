# Typed Resource Registry

**Date:** 2026-05-04
**Branch:** feat/typed-resource-registry
**Issue:** #44

## Summary

Added a second CRTP template parameter `TDerived` to `Resource<TId, TDerived>`. Each `<TId, TDerived>` pair now has its own isolated static registry map, preventing name collisions between different resource kinds (e.g., a `Shader` and a future `Texture` both keyed by `std::string` can share the same name without interfering).

As a bonus, `Get()` now returns `TDerived*` directly, removing the need for a `static_cast` at call sites.

## Changes

### Modified files

**`src/core/Resource.h`**
- Template signature: `Resource<TId>` → `Resource<TId, TDerived>`
- Static map: `std::map<TId, Resource<TId>*>` → `std::map<TId, TDerived*>`
- Constructor: stores `static_cast<TDerived*>(this)` in the map
- `Get()`: returns `TDerived*` instead of `Resource<TId>*`
- Out-of-class definition updated accordingly
- Doc comment updated to explain CRTP and the collision-prevention guarantee

**`src/abstract/Shader.h`**
- Base class: `core::Resource<std::string>` → `core::Resource<std::string, Shader>`

**`src/gldevices/GLVideoDevice.cpp`**
- `CreateShader`: `core::Resource<std::string>::Get(name)` → `abstract::Shader::Get(name)`
- Removed the now-unnecessary `static_cast<abstract::Shader*>` on the return value

## Design Decisions

### CRTP for per-type isolation
The `static_cast<TDerived*>(this)` in the `Resource` constructor is the standard CRTP pattern. It is safe because the `Resource` base constructor is called as part of constructing a `TDerived` object — the memory for the full object is already allocated and `this` points into the correct location. No virtual dispatch is involved.

### `Get()` returning `TDerived*`
Since the map now stores `TDerived*`, returning it directly is free and removes the cast burden from call sites. Callers that stored the result in a `Resource<TId>*` (none currently) would need updating, but the only caller (`GLVideoDevice::CreateShader`) already expected `Shader*`.

## Next Steps / Output to Remember

- Future resource types (`Texture`, `Material`, etc.) should follow the same pattern: `class Texture : public core::Resource<std::string, Texture>`. Each gets its own isolated registry at zero extra cost.
- `Shader::Get(name)` (inherited) is the canonical lookup — no need to reference `core::Resource<...>::Get` at call sites.

## Skills and Instructions Used

- `impl-issue` skill
- `CLAUDE.md`: git workflow, conventional commits, history file, PR
- `src/CLAUDE.md`: Google C++ style, one class per file
