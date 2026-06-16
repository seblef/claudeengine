# feat(editor): Meaningful game object naming

**Issue**: #543  
**PR**: #546  
**Branch**: `feat/editor-meaningful-object-naming-543`

## Changes

### New files
- `src/editor/ObjectNamingUtils.h` — declares `GenerateObjectName(scene, base_name)`
- `src/editor/ObjectNamingUtils.cpp` — implementation

### Modified files
- `src/editor/CMakeLists.txt` — added `ObjectNamingUtils.cpp`
- `src/editor/EditorWindow.cpp` — added `LightBaseName()` helper and name assignment on object creation

## Decisions and rationale

### Utility class vs inline logic
The naming logic was placed in a standalone `ObjectNamingUtils` utility (not inline in the panel or in `EditorScene`) to comply with the editor's GUI/logic separation rule: panels must be pure UI, editing algorithms live in dedicated classes. The function has no state, so a free function (not a class) was used.

### Naming strategy: highest existing index + 1
When there are gaps (e.g., `omni_001` and `omni_003` exist), the next object becomes `omni_004`. This is simpler than gap-filling, prevents re-using names of deleted objects (which could confuse undo/redo rename history), and matches what most DCCs do.

### Mesh name: file stem, not full path
`MeshTemplate::GetId()` returns the full path (`data/meshes/barrel01.emesh`). Using `std::filesystem::path::stem()` extracts `barrel01` — the human-readable part, consistent with how `MeshTemplate::GetAll()` already exposes names in the selection modal.

### Particle system name: template key (ps_name)
The `ps_name` variable (the template's base key, e.g., `firecamp01`) is already in scope after the modal resolves and before `tmpl->Release()` is called, so it is used directly instead of going through the template pointer.

### No change to default `name_` in `GameObject`
The default `"Object"` in `GameObject` is preserved. The editor sets a meaningful name immediately after construction; the default only shows if the object is somehow displayed before naming (which doesn't happen in normal flow).

## Output to keep in mind

- `GenerateObjectName` is a free function in `editor::` namespace — easy to extend for copy/paste naming if needed (e.g., `barrel01_copy_001`).
- The suffix format is always `_NNN` (3 digits, zero-padded). Existing maps loaded from YAML may have objects named `"Object"` or other legacy names; they are simply ignored by the prefix match and don't affect the new counter.
- Light type `kGlobal` maps to base name `global`; this is included in `LightBaseName()` but `kGlobal` lights are not user-creatable via the current toolbar (no `kCreateGlobal` tool). The case is handled defensively.

## Skills files used
- `impl-issue`

## CLAUDE.md rules especially followed
- One class per `.h`/`.cpp` pair (free function in its own file)
- GUI vs. edition logic separation: utility in standalone class, not in panel
- Include root is `src/`, all `#include` paths are project-relative
- Google C++ style guide
- Conventional commits message format
- History file stored in `history/` with timestamp prefix
