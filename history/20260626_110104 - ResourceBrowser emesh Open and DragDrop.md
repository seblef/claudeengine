# ResourceBrowser .emesh — Double-click opens MeshEditorWindow, drag-drop creates viewport instance

## Summary

`.emesh` items in **ResourceBrowser** previously opened a read-only `EmeshInfoPanel` tab on
double-click and had no drag-drop support. This change makes them behave identically to mesh
entries in `ResourcesPanel`:

- **Double-click** → opens `MeshEditorWindow` in edit mode.
- **Drag onto viewport** → emits a `MESH_TEMPLATE` payload that `EditorViewport` already handles
  to place a mesh instance at the drop position.

`EmeshInfoPanel` is deleted — it is no longer reachable.

---

## Changes

### `ResourcePanelRegistry` — new drag-drop API

Added a `DragDropSourceFn` type alias and three methods mirroring the existing open-callback API:

```cpp
using DragDropSourceFn = std::function<void(const std::filesystem::path&)>;

void RegisterDragDrop(std::string_view extension, DragDropSourceFn fn);
bool HasDragDrop(const std::filesystem::path& path) const;
void InvokeDragDrop(const std::filesystem::path& path) const;
```

Storage: `std::unordered_map<std::string, DragDropSourceFn> drag_drop_fns_`.  
The methods follow the same lookup pattern as `HasOpenCallback` / `InvokeOpenCallback`.

### `ResourceBrowser::RenderDirNode`

After the existing double-click block, added:

```cpp
if (registry_->HasDragDrop(path)) {
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        registry_->InvokeDragDrop(path);
        ImGui::EndDragDropSource();
    }
}
```

`ImGuiDragDropFlags_SourceAllowNullID` is required because `Selectable` does not hold
ImGui focus during drag; without this flag the source would never activate.

### `EditorWindow.cpp`

Replaced:

```cpp
resource_panel_registry_.Register(".emesh", [](auto path) {
    return std::make_unique<EmeshInfoPanel>(std::move(path));
});
```

With:

```cpp
resource_panel_registry_.RegisterOpenCallback(
    ".emesh",
    [this](const std::filesystem::path& path) {
        mesh_editor_->OpenEdit(path);
    });
resource_panel_registry_.RegisterDragDrop(
    ".emesh",
    [this](const std::filesystem::path& path) {
        const std::filesystem::path data_dir = core::Config::GetDataFolder();
        const std::string rel = std::filesystem::relative(path, data_dir).string();
        game::MeshTemplate* tmpl = game::MeshTemplate::GetOrLoad(rel, video_);
        if (!tmpl) return;
        ImGui::SetDragDropPayload("MESH_TEMPLATE", &tmpl, sizeof(tmpl));
        ImGui::Text("Place: %s", path.stem().string().c_str());
    });
```

`MeshTemplate::GetOrLoad` uses the data-relative path (e.g. `"meshes/cube.emesh"`) which is
the key used by the resource cache and matches the payload consumed by `EditorViewport`.

Removed `#include "editor/EmeshInfoPanel.h"`.

### `EmeshInfoPanel` deleted

`src/editor/EmeshInfoPanel.h`, `src/editor/EmeshInfoPanel.cpp`, and its entry in
`src/editor/CMakeLists.txt` are removed. No other file referenced it.

---

## Decisions

- **No changes to `EditorViewport`** — it already handles `"MESH_TEMPLATE"` payloads to place
  mesh instances; reusing the same payload type keeps both drag sources fully interchangeable.
- **`RegisterDragDrop` does not update `extensions_`** — drag-drop is a supplementary behaviour
  that does not define whether a file appears in the browser tree. Only `Register` and
  `RegisterOpenCallback` gate tree visibility via `CanOpen`.
- **`MeshTemplate::GetOrLoad` with relative path** — consistent with how ResourcesPanel
  populates its list (`GetAll()` returns templates keyed by relative path). Using the absolute
  path would create a second cache entry for the same file.

---

## Skills / CLAUDE.md notes used

- `src/editor/CLAUDE.md` — `ImGuiDragDropFlags_SourceAllowNullID` reminder; one class per file rule.
- `impl-issue` skill.
