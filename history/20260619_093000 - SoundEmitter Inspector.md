# SoundEmitter Inspector with Play Once Preview

**Issue**: #672  
**PR**: #685  
**Branch**: `feat/sound-emitter-inspector-672`

## What was done

Added an interactive Properties panel section for `GameSoundEmitter` objects in the world editor. Previously the panel was read-only (two labels). It now supports:

1. **Sound resource picker** — "Change..." button opens a `SoundEmitterSelectionModal` listing all `.sound.yaml` assets from `data/sounds/`. Confirming a selection calls `GameSoundEmitter::SetSoundName()`.
2. **Volume scale DragFloat** — live edits call `GameSoundEmitter::SetVolumeScale()`, which immediately propagates to the active `VirtualSoundInstance` when the emitter is in scene.
3. **Play Once button** — fires a one-shot flat 2D preview via the new `SoundPreviewPlayer`. Disabled when no sound is assigned.

## Files modified

| File | Change |
|---|---|
| `abstract/ISoundSource.h` | Added `SetRelative(bool)` virtual method |
| `openal/ALSoundSource.h/.cpp` | Implemented `SetRelative` via `AL_SOURCE_RELATIVE` |
| `game/GameSoundEmitter.h/.cpp` | Added `SetSoundName()` and `SetVolumeScale()` setters |
| `editor/SoundEmitterSelectionModal.h/.cpp` | Added `title` constructor parameter to avoid ImGui popup ID collision when two instances coexist |
| `editor/PropertiesPanel.h/.cpp` | `RenderSoundEmitterProperties` is now non-static/non-const and interactive; owns a `SoundEmitterSelectionModal`; fires `on_play_sound_once_` callback |
| `editor/EditorWindow.h/.cpp` | Owns `SoundPreviewPlayer`; wires `SetOnPlaySoundOnce`; ticks `sound_preview_.Update()` each frame |
| `editor/CMakeLists.txt` | Added `SoundPreviewPlayer.cpp`; linked `audio` and `openal` |

## New files

| File | Purpose |
|---|---|
| `editor/SoundPreviewPlayer.h/.cpp` | Self-contained editor audio player: initialises its own `ISoundSystem` + `ResourceManager`, keeps one `ISoundSource` in `AL_SOURCE_RELATIVE` mode for flat 2D preview playback |

## Key decisions and rationale

### `SoundPreviewPlayer` owns its own `ISoundSystem`
The editor app (`editor_app/main.cpp`) does not initialise any audio backend. Rather than adding audio to the app entrypoint (which would require threading it through `EditorSystem` → `EditorWindow` constructors), `SoundPreviewPlayer` creates its own backend internally. This keeps the change self-contained and avoids touching the game-level `SoundManager` pool. The cost is one extra OpenAL context for the editor, which is acceptable.

### `ISoundSource::SetRelative(bool)` added at the abstract level
The only reliable way to achieve no-attenuation, no-listener-position audio in OpenAL is `AL_SOURCE_RELATIVE = AL_TRUE` with position `(0,0,0)`. Exposing this at the `abstract::ISoundSource` level keeps the `SoundPreviewPlayer` backend-agnostic and future-proofs the interface for an XAudio2 backend.

### `SoundEmitterSelectionModal` parameterised with `title`
Before this change, the hardcoded popup ID `"Select Sound Template"` would collide if two `SoundEmitterSelectionModal` instances were active simultaneously (the placement modal in `EditorWindow` and the new picker in `PropertiesPanel`). Adding a constructor `title` parameter solves this cleanly with no behaviour change for existing callers.

### `GameSoundEmitter` setters are edit-time safe
All editor-placed emitters are constructed with `null` managers. `SetSoundName` stops the running instance (if any), releases the old `Sound*`, and loads the new one — but only when a `ResourceManager` is available. With null managers the setter just updates `sound_name_` for serialisation. `SetVolumeScale` similarly guards on `instance_`.

### Callback pattern (`SetOnPlaySoundOnce`)
Per the editor CLAUDE.md: panels must be pure UI. `PropertiesPanel` fires a callback; all audio logic lives in `EditorWindow::SoundPreviewPlayer`. Consistent with the existing `SetOnOpenParticleEditor` pattern.

### No undo/redo for sound name / volume scale changes
Neither `RenderParticleSystemProperties` (template label) nor other simple property editors in this codebase have undo commands for this class of change. Added as a deliberate omission to keep scope minimal; can be added in a follow-up with a `SoundEmitterPropertyCommand`.

## Output to keep in mind

- `SoundPreviewPlayer` initialises silently (logs a WARNING, sets `is_ready_ = false`) when no audio backend is available — all callers must guard on `IsReady()` or tolerate no-ops.
- `SetSoundName` on an in-scene emitter stops the looping instance but does **not** restart it. The new sound starts playing on the next `OnAddedToScene()` call. In edit mode this is fine since emitters are silent; in a live game scene a caller would need to remove + re-add the emitter to restart the loop.
- `SoundPreviewPlayer` calls `sound_system_->Update(0.f)` each frame to tick the backend. If the editor ever needs a real-time audio clock for sound effects, the dt should be plumbed through.

## Skills and CLAUDE.md rules followed

- `impl-issue` skill
- `src/CLAUDE.md`: one class per .h/.cpp, Google C++ style, include root is `src/`
- `editor/CLAUDE.md`: panel = pure UI, editing logic in dedicated class, one class per file
- `audio/CLAUDE.md`: always go through `ResourceManager::LoadSound`; `Sound*` must be `Release()`d
- `game/CLAUDE.md`: setters guard on null managers for edit-time safety
