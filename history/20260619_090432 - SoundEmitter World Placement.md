# Sound Emitter World Placement (issue #670)

## What was done

Added the ability to place `GameSoundEmitter` objects in the world from the editor.

### Files changed

| File | Change |
|---|---|
| `src/editor/SoundEmitterSelectionModal.h` | **NEW** — modal for selecting a `.sound.yaml` template |
| `src/editor/SoundEmitterSelectionModal.cpp` | **NEW** — scans `data/sounds/` and renders a list modal |
| `src/editor/EditorToolbar.h` | Added `kCreateSoundEmitter` to `EditorTool` enum; updated `IsCreationTool()` |
| `src/editor/EditorToolbar.cpp` | Added `kCreateSoundEmitter` entry (speaker icon) to `kCreationTools` |
| `src/editor/EditorWindow.h` | Forward-declared `SoundEmitterSelectionModal`; added `sound_modal_` member |
| `src/editor/EditorWindow.cpp` | Included new header + `GameSoundEmitter`; constructed modal; handled tool activation; rendered modal and instantiated emitter; added `kSoundEmitter` to copy eligibility |
| `src/editor/ObjectsPanel.cpp` | Added speaker icon for `kSoundEmitter`; added "Sound Emitters" category in the hierarchy |
| `src/editor/PropertiesPanel.h` | Forward-declared `GameSoundEmitter`; declared `RenderSoundEmitterProperties` |
| `src/editor/PropertiesPanel.cpp` | Included `GameSoundEmitter.h`; dispatched `kSoundEmitter` case; implemented read-only sound name + volume scale display |
| `src/editor/CMakeLists.txt` | Added `SoundEmitterSelectionModal.cpp` |

## Decisions & rationale

### Silent in editor, active at runtime

The editor instantiates `GameSoundEmitter` with **null** `SoundManager` and `ResourceManager`. `GameSoundEmitter` is designed to be a no-op in that case, so placement and serialization work without an audio backend. The game runtime will pass live managers so the emitters actually play.

### Selection modal mirrors ParticleSystemSelectionModal

`SoundEmitterSelectionModal` follows exactly the same pattern as `ParticleSystemSelectionModal`: `Open()` scans the asset directory, `Render()` draws the popup and returns the chosen name. This keeps the interaction model consistent.

### Properties panel is read-only

`GameSoundEmitter` exposes only getters for `sound_name_` and `volume_scale_`. Rather than adding setters (which are not required by the issue), the properties panel shows both fields as `LabelText` (read-only). Editing these values in the editor is a natural next step but was explicitly out of scope for this issue.

### Copy eligibility

`GameSoundEmitter::Copy()` was already implemented, so `kSoundEmitter` was added to the `can_copy` predicate in `EditorWindow`. This makes copy-paste work consistently with lights and meshes.

## Serialization

The `MapSerializer` already serialized and deserialized `GameSoundEmitter` (the `Visit(GameSoundEmitter&)` and `ParseSoundEmitter` functions existed from a prior issue). `MapLoader::Load` creates silent-emitter objects when managers are null, so round-trips through the editor work out of the box.

## What to keep in mind for next features

- `GameSoundEmitter` currently has no setters — if in-editor volume override editing is needed, add `SetVolumeScale(float)` and a proper undo command.
- The properties panel could offer a "Open in Sound Editor" button (analogous to "Open in Particle Editor") to jump directly to the sound template editor.
- The `data/sounds/` scanner in `SoundEmitterSelectionModal` does not recurse into subdirectories; add `recursive_directory_iterator` if subdirectory support is required.

## Skills / CLAUDE.md rules applied

- One class per `.h` / `.cpp` pair.
- `SoundEmitterSelectionModal` is pure UI — no audio logic.
- Editor code is the leaf of the dependency graph; no `editor/` headers included by game or lower layers.
- Cppcheck `unusedStructMember` suppression applied to the `entries_` member.
- Conventional commit used for the PR title.
