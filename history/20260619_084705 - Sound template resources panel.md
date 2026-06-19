# Sound Template Resources Panel (#668)

## Summary

Surfaced `.sound.yaml` templates as a browsable resource type in the editor resources panel, alongside materials, meshes, and particle effects.

## Changes

### New files

- **`src/editor/SoundEditorWindow.h/.cpp`** — Floating editor window for creating and editing `.sound.yaml` files. Toolbar row (New / Load / Save / Save As) + editable property widgets for every `audio::SoundDesc` field (audio file, loop, priority, rolloff model, min/max distance, volume, pitch). No audio preview (not in scope). File dialogs use nfd, YAML serialization uses yaml-cpp. Follows the same lifecycle pattern as `ParticleEditorWindow`.

### Modified files

- **`src/editor/ResourcesPanel.h/.cpp`** — Added a "Sounds" tree section rendered via a `std::filesystem::directory_iterator` scan of `data/sounds/*.sound.yaml`. Sorted alphabetically. Each entry shows `ICON_FA_MUSIC`. Double-clicking calls the new `on_sound_open_` callback. A `+` button opens a "New Sound Template" modal (same pattern as "New Material"). Added `SetOnSoundOpen` and `SetOnNewSound` setters.

- **`src/editor/EditorWindow.h/.cpp`** — Added `std::unique_ptr<SoundEditorWindow> sound_editor_` and `bool show_sound_editor_`. Wired `SetOnSoundOpen` and `SetOnNewSound` callbacks: the "new" callback writes a default `.sound.yaml` to disk before calling `OpenTemplate`. Added an "Audio" menu in the menu bar with a "Sound Editor" toggle item.

- **`src/editor/MapSerializer.h/.cpp`** — Fixed pre-existing build break: `SerializeVisitor` was missing the pure-virtual `Visit(GameSoundEmitter&)` override introduced in PR #666. Implemented it to emit `type: sound_emitter`, `sound`, `volume_scale`, and `transform` — matching the format that `MapLoader` already parses.

- **`src/editor/CMakeLists.txt`** — Added `SoundEditorWindow.cpp` to the `editor` static library.

## Design decisions

### Filesystem scan for the Sounds section

Unlike materials/meshes (shown from the in-memory registry), sound templates are discovered by scanning `data/sounds/` on disk each `Render()` call. Rationale: sounds are not pre-loaded at startup, so the in-memory registry would always be empty until the user loads them. Scanning is cheap for a small directory and lets the user see all available templates immediately after creation.

### No audio preview in SoundEditorWindow

The issue does not ask for playback. Adding a preview would require access to an `ISoundSystem*` (owned by the game layer, not available in the editor layer at this scope). Deferred to a future issue.

### New sound template creation in EditorWindow

The default file is written in `EditorWindow`'s `SetOnNewSound` callback rather than inside `SoundEditorWindow`, because the editor owns the data folder path and the callback pattern in `ResourcesPanel` is consistent with how "New Material" works. `OpenTemplate` then loads the just-created file.

### MapSerializer fix bundled in same PR

The `Visit(GameSoundEmitter&)` stub was a compile blocker introduced by PR #666 and not caught because `MapSerializer` was not rebuilt against the new `GameObjectVisitor`. The fix is minimal (matches the MapLoader deserialization keys exactly) and is a prerequisite for the editor to compile.

## Output to keep in mind

- `SoundEditorWindow` has no save-on-close prompt. If the user closes it with unsaved edits they lose them. A future issue could add this.
- The Sounds panel does not reload live `audio::Sound` instances when a file is saved from the editor. If the user edits a sound that is currently playing in the scene, they must reload the map to hear the changes.
- `data/sounds/` must exist for the directory scan to work without an error (the `std::error_code` overload of `directory_iterator` is used to suppress exceptions).

## Skills and instructions used

- `impl-issue` skill
- `src/editor/CLAUDE.md`: GUI vs. edition logic separation, one class per file pair
- `src/CLAUDE.md`: include root, Google C++ style, one class per file
- `CLAUDE.md` (root): conventional commits, history file, cpplint, PR to `dev`
