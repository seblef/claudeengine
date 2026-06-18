# Audio Abstract Interfaces — Issue #660

**Branch:** `feat/audio-abstract-interfaces-660`  
**PR:** #674 → `dev`  
**Date:** 2026-06-18

---

## Changes

### New files in `src/abstract/`

| File | Purpose |
|---|---|
| `AudioFormat.h` | `enum class AudioFormat` — PCM sample layout (Mono8, Mono16, Stereo8, Stereo16) |
| `ISoundBuffer.h` | Opaque handle to device-resident PCM data; exposes `GetNumFrames`, `GetNumChannels`, `GetSampleRate` |
| `ISoundSource.h` | Playing instance: 3D position/velocity, gain, pitch, loop, play/pause/stop |
| `ISoundSystem.h` | Backend contract: Initialize/Shutdown, Update(dt), SetListenerTransform(Mat4f), CreateBuffer, CreateSource |

### New module `src/audio/`

| File | Purpose |
|---|---|
| `SoundSystemFactory.h` | Platform-select factory declaration |
| `SoundSystemFactory.cpp` | Stub `Create()` returning `nullptr` until concrete backends exist |
| `CMakeLists.txt` | Static library linking `abstract` |

### Modified

- `src/CMakeLists.txt` — added `add_subdirectory(audio)` between `abstract` and `mesh`

---

## Design Decisions & Rationale

### Listener transform as `core::Mat4f`

The issue said "set listener transform" (singular), which implies a full world-space transform. Using `Mat4f` is consistent with `physics::IPhysicsBodyListener::OnBodyTransformUpdated(const core::Mat4f&)`. Concrete backends extract position and orientation from matrix columns.

### ISoundBuffer is opaque

PCM data flows in at construction time via `ISoundSystem::CreateBuffer(data, size_bytes, format, sample_rate)`. The buffer handle exposes only read-only metadata. This keeps the abstraction tight — callers can query duration or sample rate without needing to remember what they passed at construction.

### Factory in a separate `audio` module

`abstract` is a header-only INTERFACE CMake target (no `.cpp` files, no linker output). The factory `Create()` needs a `.cpp` with `#ifdef` platform guards or concrete class includes. Putting it in a separate `audio` static library keeps the dependency chain:

```
audio → abstract → core
```

Future concrete backends (e.g. `openal_audio`) will link against `audio` or replace the factory implementation entirely.

### Stub returns nullptr

No concrete audio backend exists for this milestone issue. The factory is the correct integration point, and future issues will either add `#ifdef` branches or replace the file. Returning `nullptr` is explicit and forces callers to handle the "no audio" case.

### `ISoundSource::SetVelocity` added beyond spec

The issue listed "3D position, gain, pitch, play/pause/stop, loop flag". Velocity was added because:
1. All spatial audio APIs (OpenAL, XAudio2) require velocity for Doppler effect
2. Omitting it now would force a breaking interface change later
3. It adds no implementation burden at this stage (pure virtual)

---

## Output for next issues

- The `audio::SoundSystemFactory::Create()` stub is the integration point for concrete backend issues (OpenAL on Linux, XAudio2 on Windows)
- Concrete backends should implement `abstract::ISoundSystem`, `abstract::ISoundBuffer`, `abstract::ISoundSource`
- The `audio` CMake library should eventually link concrete backend targets
- `SetListenerTransform` convention: position = column 3, forward = -column 2, up = column 1 (OpenGL convention matching the engine's camera)
- No new CMake options needed for audio yet (no optional dependency)

---

## Skills & CLAUDE.md instructions followed

- **Skills used:** `impl-issue`
- **CLAUDE.md rules applied:**
  - One class per `.h` / `.cpp` pair (each interface in its own file)
  - Google C++ style guide (cpplint passes with 0 warnings)
  - Include root is `src/` — all paths are project-relative
  - New module has a `CMakeLists.txt` and entry in `src/CMakeLists.txt`
  - Branch prefixed `feat/`, commit follows Conventional Commits
  - PR opened to `dev` with `Close #660`
  - History file written to `history/`
  - Always checkout `dev` and pull before starting
