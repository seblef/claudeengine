# OpenAL Soft Backend — Issue #661

**Branch:** `feat/openal-soft-backend-661`  
**PR:** → `dev`  
**Date:** 2026-06-18

---

## Changes

### New module `src/openal/`

| File | Purpose |
|---|---|
| `ALSoundBuffer.h/.cpp` | `ISoundBuffer` backed by an OpenAL buffer object (`alGenBuffers` / `alBufferData`) |
| `ALSoundSource.h/.cpp` | `ISoundSource` backed by an OpenAL source object; maps position, velocity, gain, pitch, loop, and playback state to AL source properties |
| `ALSoundSystem.h/.cpp` | `ISoundSystem` that opens the default AL device, creates and activates an AL context, and exposes buffer/source factories |
| `CMakeLists.txt` | Static library `openal_audio`; locates system OpenAL via `find_package(OpenAL REQUIRED)` |

### Modified

| File | Change |
|---|---|
| `src/CMakeLists.txt` | Added `add_subdirectory(openal)` (conditional on `UNIX`) before `audio` so `openal_audio` is defined when `audio` links against it |
| `src/audio/CMakeLists.txt` | Added `target_link_libraries(audio PRIVATE openal_audio)` on `UNIX` |
| `src/audio/SoundSystemFactory.cpp` | Replaced `nullptr` stub with `std::make_unique<openal::ALSoundSystem>()` under `#ifdef __linux__` |

---

## Design Decisions & Rationale

### Separate `openal` module (mirrors `gldevices`)

The concrete OpenAL backend lives in `src/openal/` following the same pattern as `src/gldevices/` for rendering. The `audio` module remains the thin platform-routing layer (just the factory), keeping concern boundaries clean:

```
app → audio → openal_audio → abstract → core
                           └─ OpenAL::OpenAL (system)
```

### `find_package(OpenAL REQUIRED)` — system library, not FetchContent

OpenAL Soft ships as a standard system package (`libopenal-dev` on Ubuntu). Linking via the CMake `OpenAL::OpenAL` imported target means CI/developer machines only need the OS package manager, with no per-project download. On Windows, a future XAudio2 backend will be added under `#if defined(_WIN32)` without touching the Linux path.

### Listener orientation convention

`SetListenerTransform` follows the convention established in the previous PR's history note:
- Position = column 3 of the Mat4f (row-major → `transform(row, 3)`)
- Forward = **−Z column** (`-transform(r, 2)`) — OpenGL convention, camera looks down −Z
- Up = **+Y column** (`transform(r, 1)`)

These six floats are packed into `AL_ORIENTATION` in the order OpenAL expects: `[forward, up]`.

### `Update(float)` is a no-op

OpenAL Soft mixes and spatializes audio on its own mixing thread. There is no per-frame pump or streaming callback needed for static (non-streaming) buffers. The override is kept to satisfy the interface contract and as a future hook for streaming audio.

### `IsValid()` guard pattern

Both `ALSoundBuffer` and `ALSoundSource` expose `IsValid()` so `ALSoundSystem` can detect construction failures without exceptions. This matches the engine's error-handling style (no exceptions in renderer/physics code).

### No `alGetError` loop in `ALSoundSource`

AL source creation errors are detected at the device level, not per-source. Checking `alGetError()` after `alGenSources` would catch driver-level OOM, but that scenario is already reported by the device. Keeping it simple reduces noise; the `IsValid()` check on `source_id_ != 0` covers the most common failure.

---

## Output for Next Issues

- `SoundSystemFactory::Create()` now returns a fully initialized `ALSoundSystem` on Linux; callers must still call `Initialize()` before use.
- macOS support for OpenAL can be added by extending the `#ifdef __linux__` guard to `#if defined(__linux__) || defined(__APPLE__)` (OpenAL is a framework on macOS).
- Windows XAudio2 backend: add `src/xaudio2/` module, mirror the CMake pattern with `if(WIN32)`, and add `#elif defined(_WIN32)` branch in the factory.
- Streaming audio (loading large files progressively via `alBufferData` in `Update`) would require making `Update` non-trivial; the hook is already in place.
- `SoundSystemFactory.h` comment block listing platform routing should be updated once more backends are added.

---

## README.md Proposal

Add a **Dependencies** or **Audio** section noting that Linux builds require OpenAL Soft:

```
sudo apt install libopenal-dev   # Ubuntu / Debian
```

This is the only new system dependency introduced by this issue.

---

## CLAUDE.md Proposals

None — the existing instructions were sufficient.

---

## Specification Improvement Proposals

- Clarify whether `ISoundSystem::Initialize()` failure should leave the factory returning a partially constructed object or a `nullptr`. Currently the factory returns a valid `ALSoundSystem` and the caller must check `Initialize()` — an alternative would be for the factory to call `Initialize()` internally and return `nullptr` on failure.

---

## Skills & CLAUDE.md Instructions Followed

- **Skills used:** `impl-issue`
- **CLAUDE.md rules applied:**
  - Always checkout `dev` and pull before starting
  - One class per `.h` / `.cpp` pair
  - Google C++ style guide; cpplint passes with 0 warnings
  - Include root is `src/` — all `#include` paths are project-relative
  - New module has a `CMakeLists.txt` and entry in `src/CMakeLists.txt`
  - `cppcheck-suppress unusedStructMember` on all class-level pointer/value members
  - Branch prefixed `feat/`, commit follows Conventional Commits
  - PR opened to `dev` with `Close #661`
  - History file written to `history/`
