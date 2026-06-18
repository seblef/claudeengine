# CLAUDE.md — audio module

## Role

The `audio` module owns sound resource loading, decoding, and backend selection. It sits between the `abstract` audio interfaces and the `game` layer that attaches sounds to game objects.

## Dependency graph

```
game → audio → abstract → core
```

The `audio` module must not depend on `game`, `renderer`, `editor`, or any platform-specific headers. Go through `abstract/ISoundSystem.h` and `abstract/ISoundBuffer.h` for all backend interaction.

## Module structure

| File(s) | Responsibility |
|---|---|
| `SoundSystemFactory` | Selects and instantiates the platform audio backend (OpenAL on Linux, XAudio2 on Windows) |
| `Sound` | Reference-counted resource (`core::Resource<string, Sound>`) — loads `.sound.yaml`, decodes audio, creates `ISoundBuffer` |
| `SoundDesc` | POD descriptor parsed from `.sound.yaml`: file, loop, priority, rolloff, distances, volume, pitch |
| `RolloffType` | Enum for distance-attenuation models: `kLinear`, `kInverse`, `kExponential` |
| `AudioDecoderUtils` | Free-function decoder utility: `DecodeAudioFile(path)` → `DecodedAudio`; supports WAV, MP3, FLAC, OGG/Vorbis, OPUS (optional) |
| `ResourceManager` | Central access point for Sound resources — wraps `Sound::Get`/`Sound::GetOrLoad` and holds the `ISoundSystem*` for buffer creation and hot-reload |
| `VirtualSoundInstance` | Handle to a logical in-world sound event; decoupled from hardware sources. Obtained via `SoundManager::PlaySound()` |
| `VirtualSoundState` | Enum: `kPlaying` (has hardware source), `kSuspended` (no source, will restart when one frees), `kStopped` (finished) |
| `SoundManager` | Per-frame virtual-to-hardware source multiplexer: distance cull → priority rank → pool assignment |

## Loading sounds — always go through ResourceManager

**Do not call `Sound::GetOrLoad` directly.** Use `ResourceManager::LoadSound` instead so the `ISoundSystem*` lifetime is managed in one place:

```cpp
audio::ResourceManager mgr(sound_system);

// Primary load path — AddRef'd; caller must Release():
Sound* s = mgr.LoadSound("explosion");
src->SetBuffer(s->GetBuffer());
s->Release();

// Check if a sound is already resident without triggering a load:
Sound* s = mgr.GetSound("explosion");  // nullptr if not loaded

// Hot-reload after an asset file changes on disk:
mgr.Reload("explosion");  // Sound* pointers stay valid; re-fetch GetBuffer()
```

## Asset convention

Sound assets live in `data/sounds/{name}.sound.yaml`. The `file` field inside the YAML is relative to the engine data folder:

```yaml
file: sounds/explosion.wav
loop: false
volume: 0.8
rolloff: inverse
min_distance: 1.0
max_distance: 50.0
```

## Decoder support

All decoders are in `AudioDecoderUtils.cpp` (the "one `*Utils.cpp`" rule). They all output signed 16-bit PCM:

| Format | Library | Availability |
|---|---|---|
| WAV | dr_wav | Always |
| MP3 | dr_mp3 | Always |
| FLAC | dr_flac | Always |
| OGG/Vorbis | stb_vorbis | Always |
| OPUS | opusfile | Optional — `HAVE_OPUSFILE` flag |

## Playing sounds — always go through SoundManager

```cpp
audio::SoundManager mgr(sound_system, 32);  // 32 hardware sources

// Each frame:
mgr.SetListenerTransform(camera_matrix);    // must be called before Update()
mgr.Update(dt);

// Spawn a one-shot:
Sound* s = resources.LoadSound("explosion");
VirtualSoundInstance* h = mgr.PlaySound(s, emitter_pos);
s->Release();  // SoundManager holds its own ref

// Looping ambient emitter:
VirtualSoundInstance* h = mgr.PlaySound(s, pos, /*loop=*/true, /*priority=*/1);

// Move emitter each frame:
h->SetPosition(new_pos);

// Stop early:
h->Stop();  // handle is invalid after next mgr.Update()
```

## Key invariants

- `Sound::GetOrLoad` / `Release` must be balanced. `ResourceManager::LoadSound` and `ResourceManager::GetSound` both return AddRef'd pointers.
- After `ResourceManager::Reload`, the `ISoundBuffer` object is destroyed and recreated. Callers that cached `Sound::GetBuffer()` must re-fetch the pointer.
- `ResourceManager` holds a raw `ISoundSystem*` — it must not outlive the sound system.
- `VirtualSoundInstance*` handles are valid until `Stop()` has been called **and** `SoundManager::Update()` has run, or until the sound ends naturally. Do not dereference a handle after `IsStopped()` returns true.
- `SoundManager::SetListenerTransform()` must be called before `Update()` each frame for distance culling to use the current listener position.
- Suspended (silenced) instances will **restart from the beginning** when they re-acquire a hardware source — there is no seek/resume. This is expected for looping emitters; one-shot sounds should be stopped explicitly if they must not replay.

## Guidelines

Follow all rules in `src/CLAUDE.md`. Additionally:
- One class per `.h` / `.cpp` pair.
- Free functions (decoders, format helpers) belong in `AudioDecoderUtils`.
- Do not include platform or OpenAL headers directly; go through `abstract/`.
