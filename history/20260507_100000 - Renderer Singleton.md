# Renderer Singleton

**Date:** 2026-05-07
**Branch:** feat/renderer-singleton
**Issue:** #75

## Summary

Converted `renderer::Renderer` to a singleton using a redesigned `core::Singleton<T>` base class. The new template uses `new`-based creation (TSingleton pattern) so that singletons with constructor arguments are supported and creation order is caller-controlled. `core::EventManager` was updated to use the same pattern.

## Changes

### Modified: `src/core/Singleton.h`

Full rewrite from Meyer's static local to TSingleton-style pointer registration:

- **Before**: `static T instance;` — lazy creation on first `Instance()` call, no constructor arguments, no controlled destruction
- **After**: `static T* instance_` — registered in base constructor via `static_cast<T*>(this)`, cleared in base destructor; caller uses `new T(args)` to create and `Shutdown()` to destroy

New API:
- `Singleton()` — registers `this` as the instance; asserts on double-creation
- `~Singleton()` — clears `instance_`
- `static T& Instance()` — returns `*instance_`; asserts if not instanced
- `static void Shutdown()` — `delete instance_`; destructor clears pointer (safe to call twice)
- `static bool IsInstanced()` — null check

`static_cast<T*>(this)` replaces the original `long`-offset trick (`(long)(T*)1 - (long)(TSingleton<T>*)(T*)1`). Both compute the same pointer adjustment for the base→derived offset; `static_cast` is portable and UB-free.

### Modified: `src/core/EventManager.h`

- `EventManager() = default;` moved from `private` to `public` — required for `new EventManager()` at the call site
- Removed `friend class Singleton<EventManager>` — no longer needed with the new pattern
- Updated usage doc comment

### Modified: `src/renderer/Renderer.h`

- Added `#include "core/Singleton.h"`
- Inherits from `core::Singleton<Renderer>`
- `explicit Renderer(abstract::VideoDevice* video)` is now `public` (caller creates with `new`)
- Removed custom `Init()`, `Instance()`, `Shutdown()` declarations and `static Renderer* instance_` — all provided by `Singleton<Renderer>` now

### `src/renderer/Renderer.cpp`

No changes — no custom singleton methods were present after #74.

### Modified: `src/app/main.cpp`

- `new core::EventManager()` added after `AppConfig::Init`
- `renderer::Renderer renderer(video)` replaced by `new renderer::Renderer(video)`
- `renderer.Update/SetRenderableInfos` replaced by `renderer::Renderer::Instance().Update/SetRenderableInfos`
- `renderer::Renderer::Shutdown()` and `core::EventManager::Shutdown()` added in cleanup (reverse creation order)

## Decisions & Rationale

### `new`-based creation instead of lazy `Init(Args...)`
The user explicitly wants `new` for singleton creation to control order and pass constructor arguments. The TSingleton pattern achieves this: the base class constructor registers `this` when `new T(args...)` is called.

### Constructors become public (assertion-based protection)
`new T(args...)` must be callable from the creation site. Private constructors are replaced by a runtime `assert(!instance_)` that fires on double-creation. This is the same protection the original TSingleton provided.

### `static_cast<T*>(this)` over the `long` trick
The original `(long)(T*)1 - (long)(TSingleton<T>*)(T*)1` offset is the Ogre3D approach for multi-inheritance pointer adjustment. `static_cast<T*>(this)` computes the same adjustment at the language level, is portable across 32/64-bit platforms, and has no UB. Storing the result for post-construction use is safe.

### `Shutdown()` via `delete instance_`
`delete instance_` triggers `~T()` → `~Singleton<T>()` which sets `instance_ = nullptr`. A second `Shutdown()` call then `delete`s `nullptr` — a C++ no-op. No manual null guard needed in `Shutdown()`.

### No virtual destructor needed
`instance_` is `T*`. Deleting via the derived pointer calls `~T()` directly; no base-class virtual dispatch is involved.

## Skills & Instructions Used

- `impl-issue` skill (CLAUDE.md git workflow)
- Root `CLAUDE.md`: conventional commits, history file, cpplint, PR to dev
- `src/CLAUDE.md`: one class per file, Google C++ style, include root is `src/`
