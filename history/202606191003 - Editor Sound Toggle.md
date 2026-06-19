# Editor Sound Toggle — Issue #673

## Summary

Added a global sound enable/disable toggle to the main editor toolbar. When enabled,
all `GameSoundEmitter` objects in the scene loop their positional audio, and the
listener position/orientation is updated from the viewport camera each frame.
When disabled, the system is fully silent: all instances are stopped, hardware
sources are idle, and the listener is not updated. The Properties panel "Play Once"
button continues to work regardless of the toggle, as it uses the independent
`SoundPreviewPlayer`.

## Changes

### `src/game/GameSoundEmitter.h / .cpp`

- Added `bool in_scene_` member to track scene membership, set in
  `OnAddedToScene()` and cleared in `OnRemovedFromScene()`.
- Added `SetManagers(SoundManager*, ResourceManager*)`: stops any running
  instance, releases the current sound buffer, reloads from the new
  resource manager, and restarts looping playback if `in_scene_` is true.
  Passing null pointers silences the emitter without removing it from the scene.
  This avoids going through the full scene lifecycle to wire/unwire audio.

### `src/editor/EditorToolbar.h / .cpp`

- Added `sound_enabled_` bool and `on_sound_toggle_` callback.
- Added `SetOnSoundToggle()`, `SetSoundEnabled()`, `IsSoundEnabled()`.
- Rendered a speaker toggle button (`ICON_FA_VOLUME_HIGH` / `ICON_FA_VOLUME_XMARK`)
  after the group buttons, styled with the teal active colour when enabled.

### `src/editor/EditorViewport.h / .cpp`

- Added `GetCameraWorldTransform()`: returns the viewport camera's
  `GameObject::GetWorldTransform()` so `EditorWindow` can feed it to
  `SoundManager::SetListenerTransform()` each frame.

### `src/editor/EditorWindow.h / .cpp`

- Added audio members: `editor_sound_system_`, `editor_sound_manager_`,
  `editor_sound_resources_` (all owned by `EditorWindow`).
- Initialises the editor audio backend in the constructor using
  `SoundSystemFactory::Create()`. Logs a warning and degrades gracefully
  if the system is unavailable.
- `SetOnSoundToggle` callback wires to `EnableSceneSound()` / `DisableSceneSound()`.
- `EnableSceneSound()`: calls `SetManagers(real_mgr, real_res)` on every
  `kSoundEmitter` object in the scene.
- `DisableSceneSound()`: calls `StopAll()` on the sound manager, then
  `SetManagers(nullptr, nullptr)` on every emitter.
- Per-frame (in `Render()`): when sound is enabled, calls
  `SetListenerTransform(viewport camera)` then `SoundManager::Update(dt)`.
- Sound emitter placement (via `SoundEmitterSelectionModal`) passes real managers
  when the toggle is on, null when off.
- After `LoadMap()`: calls `EnableSceneSound()` if the toggle is on, so
  emitters loaded from a map file are immediately activated.
- Destructor calls `DisableSceneSound()` to null out all emitter manager pointers
  before the scene is destroyed, preventing dangling pointer access during
  `GameSoundEmitter::OnRemovedFromScene()`.

## Key Decisions

**Why `SetManagers()` instead of calling `OnRemovedFromScene()` / `OnAddedToScene()` directly?**
Those callbacks are part of the scene lifecycle. Calling them out-of-sequence
would set `in_scene_` incorrectly and risk re-registering renderer resources on
`GameMesh` subclasses (though `GameSoundEmitter` has no renderer resources, it's
still a violation of the contract). A dedicated `SetManagers()` is explicit and safe.

**Why initialize a second `ISoundSystem` instead of sharing `SoundPreviewPlayer`'s system?**
`SoundPreviewPlayer` uses `AL_SOURCE_RELATIVE` for flat 2D preview with no listener
and no distance attenuation. The editor 3D system needs a proper listener transform
and distance culling. Sharing the backend would require changes to how sources are
configured.

**Destruction order safety**: `editor_sound_system_` and friends are declared after
`scene_` in `EditorWindow.h`, so they would be destroyed before `scene_` during
member destruction. This is handled by `DisableSceneSound()` in the destructor,
which nulls out all emitter manager pointers before the members are destroyed.

## Skills / Instructions Used

- `src/editor/CLAUDE.md`: panel vs. editing logic separation (toggle logic in
  `EditorWindow`, UI only in `EditorToolbar`)
- `src/audio/CLAUDE.md`: always go through `SoundManager` and `ResourceManager`;
  `SetListenerTransform` before `Update` each frame
- `src/CLAUDE.md`: one class per file, `cppcheck-suppress unusedStructMember`
  for new private members, Google C++ style

## Output to Keep in Mind

- When a map is loaded while the sound toggle is ON, `EnableSceneSound()` is called
  automatically after `LoadMap()`. No extra step needed.
- `DisableSceneSound()` is idempotent (guards against null scene and null manager).
- If the audio backend fails to initialise, the toggle button still appears but
  is a no-op (logged at WARNING level).
