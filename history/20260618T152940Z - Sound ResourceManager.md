# Sound ResourceManager (#664)

## Summary

Added `audio::ResourceManager` as the central access point for `Sound` resources, and extended `Sound` with a `Reload()` method to support hot-reload without invalidating existing pointers.

## New files

| File | Purpose |
|---|---|
| `src/audio/ResourceManager.h` | `audio::ResourceManager` — wraps `Sound::Get` / `Sound::GetOrLoad` and holds `ISoundSystem*` for buffer creation and reload |
| `src/audio/ResourceManager.cpp` | Implementation of `GetSound`, `LoadSound`, and `Reload` |

## Modified files

| File | Change |
|---|---|
| `src/audio/Sound.h` | Added `Reload(ISoundSystem*)` declaration |
| `src/audio/Sound.cpp` | Implemented `Reload()`: re-parses YAML, resets buffer, calls `InitFromDesc` |
| `src/audio/CMakeLists.txt` | Added `ResourceManager.cpp` to `audio` library |

## API

```cpp
audio::ResourceManager mgr(sound_system);

// Retrieve existing (AddRef'd) or nullptr:
Sound* s = mgr.GetSound("explosion");

// Load-or-get (AddRef'd), the primary call site:
Sound* s = mgr.LoadSound("explosion");
s->Release();  // when done

// Hot-reload (re-parses YAML, recreates ISoundBuffer, Sound* stays valid):
mgr.Reload("explosion");
```

## Key design decisions

**`GetSound` vs `LoadSound` split**
`GetSound` is a pure registry lookup (`Sound::Get + AddRef`) that returns nullptr when the asset is not loaded — useful for editor panels checking which sounds are resident. `LoadSound` delegates to `Sound::GetOrLoad`, which loads on first call and AddRef's on subsequent ones, the primary call site for gameplay code.

**`Reload` via `Sound::Reload(ISoundSystem*)`**
Rather than storing `ISoundSystem*` in every `Sound` object, the pointer is passed at reload time by the `ResourceManager`. This keeps `Sound` lean (no retained backend pointer needed at runtime) and concentrates the reload path in the manager that already owns the ISoundSystem lifetime.

**Placed in `audio` module**
`Sound` lives in `audio`; the ResourceManager wraps it, so it belongs in the same module. No game-layer dependency is introduced.

## Output to keep in mind

- `ResourceManager` holds a raw `ISoundSystem*` — it must not outlive the system.
- After `Reload()`, callers that cached `Sound::GetBuffer()` must re-fetch the pointer (the `ISoundBuffer` object is destroyed and recreated).
- Future `SoundManager` (#665) should hold a `ResourceManager` instance and forward `LoadSound` calls through it.

## Skills used

- `impl-issue`

## Instructions especially taken into account

- `src/CLAUDE.md`: one class per `.h`/`.cpp`; Google C++ style; include root is `src/`
- `CLAUDE.md` (root): feat/ branch from dev, cpplint before commit, conventional commit, PR with close command
- `src/audio/` patterns: `cppcheck-suppress unusedStructMember` on raw pointer members
