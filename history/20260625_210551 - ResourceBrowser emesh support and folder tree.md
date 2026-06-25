# ResourceBrowser — .emesh support and per-extension folder hierarchy

**Issue:** #800
**Branch:** feat/resource-browser-emesh-folder-tree

## Changes

### New: `EmeshInfoPanel` (`src/editor/EmeshInfoPanel.h/.cpp`)

A read-only `IResourcePanel` implementation for `.emesh` files. On construction
it invokes `mesh::EmeshReader::Read()` and caches the `MeshData`. `Draw()` renders
vertex count, index count, the AABB (min/max), and the per-submesh material paths.
`IsDirty()` always returns `false`; `Save()` is a no-op — consistent with the
read-only contract described in the issue.

### Modified: `src/editor/ResourceBrowser.h` and `.cpp`

**Data model** — replaced the flat `file_map_`:
```cpp
std::unordered_map<std::string, std::vector<std::filesystem::path>> file_map_;
```
with a per-extension directory tree:
```cpp
struct DirNode {
  std::map<std::string, DirNode>     children;  // sorted by name
  std::vector<std::filesystem::path> files;
};
std::unordered_map<std::string, DirNode> tree_map_;
```
`std::map` for `children` keeps siblings sorted without an explicit sort pass at
the directory level.

**`Scan()`** — after matching each discovered path against a registered extension,
computes the path relative to `data/`, walks every directory component (excluding
the filename) to build/reach the correct `DirNode`, then appends the absolute path
to `files`. A post-pass `sort_node` lambda sorts `files` at every node recursively.

**`RenderTree()`** — the flat file loop is replaced by `RenderDirNode()`:
```cpp
void RenderDirNode(const DirNode& node, const std::string& ext,
                   const std::string& id_prefix);
```
Children are rendered as `ImGui::TreeNode` (collapsed by default); files are
`ImGui::Selectable` leaves with the existing double-click → `OpenOrFocus` behaviour.
The `id_prefix` prevents ImGui ID collisions across extensions and sibling dirs.
An `ImGui::Indent/Unindent` pair wraps the top-level call to align the tree
visually under the section header.

### Modified: `src/editor/EditorWindow.cpp`

Added `#include "editor/EmeshInfoPanel.h"` and the registry call:
```cpp
resource_panel_registry_.Register(
    ".emesh",
    [](std::filesystem::path path) -> std::unique_ptr<IResourcePanel> {
      return std::make_unique<EmeshInfoPanel>(std::move(path));
    });
```
Registered before the vehicle entry so "emesh" appears first in the browser.

### Modified: `src/editor/CMakeLists.txt`

Added `EmeshInfoPanel.cpp` to the `editor` static library sources.

## Decisions

- **`std::map` for children** (ordered) rather than `std::unordered_map` (insertion
  order) — siblings are naturally sorted, no secondary pass required.
- **`id_prefix` for `TreeNode`** — ImGui tree nodes need unique string IDs.
  Concatenating the path components gives globally unique IDs with no extra state.
- **`ImGui::Indent/Unindent`** around `RenderDirNode` — visual cue to distinguish
  the root-level dir nodes from the section header.
- **No "New" button for `.emesh`** — authored externally; `RegisterNew` is not
  called, so `CanCreate(".emesh")` returns false and the button is suppressed.

## Keep in mind

- The `EmeshInfoPanel` reads the full mesh into memory on construction (including
  all vertex and index data). For very large meshes this is heavier than a
  header-only read. If that becomes a concern, a lightweight header-only reader
  should be extracted from `EmeshReader`.
- The `DirNode` approach assumes a reasonably shallow directory tree under `data/`.
  For very large trees (thousands of files) the recursive `sort_node` and
  `RenderDirNode` are still O(n log n) and O(n) respectively, so they remain
  acceptable.

## Skills / instructions consulted

- `impl-issue` skill
- `src/CLAUDE.md` — one class per file, Google C++ style, include paths from `src/`
- `src/editor/CLAUDE.md` — `IResourcePanel` contract, `std::find_if` preference,
  value members require full includes
