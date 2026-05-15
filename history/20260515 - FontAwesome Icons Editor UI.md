# Font Awesome Icons — Editor UI

**Date:** 2026-05-15
**Branch:** `feat/fa-icons-editor`
**Issue:** #198

## Summary

Replaced placeholder text labels and `ImGui::ColorButton` squares in the editor UI with
Font Awesome 6 glyphs embedded as a merged ImGui font.

## Changes

### Assets
- `data/fonts/fa-solid-900.ttf` — Font Awesome 6 Free Solid font (417 KB), committed to
  the repository so the editor works out-of-the-box without a network step.
- `external/IconsFontAwesome6.h` — header-only codepoint macros from
  [IconFontCppHeaders](https://github.com/juliettef/IconFontCppHeaders).

### `src/editor/CMakeLists.txt`
Added `${CMAKE_SOURCE_DIR}/external` to the `editor` target's include directories so
`#include <IconsFontAwesome6.h>` resolves from any editor source file.

### `src/editor/EditorSystem.cpp`
Font merge in the constructor, right after `ImGui::StyleColorsDark()`:
- Calls `io.Fonts->AddFontDefault()` to keep the default text font.
- Merges the icon font at 13 px (`MergeMode = true`, `PixelSnapH = true`,
  `GlyphMinAdvanceX = 13`).
- Resolves the `.ttf` path via `core::Config::GetDataFolder()` so it works regardless
  of the working directory the binary is launched from.
- Calls `io.Fonts->Build()` explicitly after adding all fonts.

### `src/editor/EditorToolbar.cpp`
- Extended `ToolEntry` with a `tooltip` field.
- Replaced `[S]`/`[C]`/`[T]`/`[R]`/`[Z]` labels with FA icon macros:

  | Tool | Icon |
  |---|---|
  | Select | `ICON_FA_ARROW_POINTER` |
  | Camera | `ICON_FA_VIDEO` |
  | Translate | `ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT` |
  | Rotate | `ICON_FA_ROTATE` |
  | Scale | `ICON_FA_EXPAND` |

- Added `ImGui::SetItemTooltip("%s", kTools[i].tooltip)` after each button so the tool
  name and keyboard shortcut remain discoverable ("Select (Q)", "Camera (C)", etc.).

### `src/editor/ObjectsPanel.cpp`
- Removed `ImGui::ColorButton` + `ImGui::SameLine()` from `RenderGroup()`.
- `RenderGroup()` now takes `const char* icon` instead of a button ID + color.
- Group header label: `std::string(icon) + " " + group_name` passed to `TreeNodeEx`.
- Leaf label: `std::string(icon) + " " + obj->GetName()` — same icon as the group.
- Removed now-unused `kIconSize` / `kIconFlags` constants.

  | Group | Icon |
  |---|---|
  | Meshes | `ICON_FA_CUBE` |
  | Lights | `ICON_FA_LIGHTBULB` |
  | Cameras | `ICON_FA_CAMERA` |

### `src/editor/ResourcesPanel.cpp`
Same treatment as ObjectsPanel — no helper function needed since only two groups:
- Materials: group header + each leaf prefixed with `ICON_FA_PALETTE`.
- Meshes: group header + each leaf prefixed with `ICON_FA_CUBE`.
- Removed now-unused `kIconSize` / `kIconFlags` constants.

## Decisions

**Why commit the TTF rather than FetchContent?**  Binary blobs fetched at build time
add latency and require network access on every clean build. At 417 KB the font is
small enough to live in the repository alongside other data assets.

**Why `external/` for `IconsFontAwesome6.h`?**  It follows the existing convention for
third-party headers (ImGuizmo, fast_obj, stb all land in `external/`).

**Why `ICON_FA_VIDEO` for the camera tool instead of `ICON_FA_CAMERA`?**  Avoids a
visual collision with the camera *objects* in ObjectsPanel which use `ICON_FA_CAMERA`.

**Why `MergeMode = true`?** Merging into the default font lets ImGui render icon glyphs
and Latin text in the same `ImGui::Text()` / button label string, which is the standard
pattern for inline icons.

## Output to keep in mind

- The font file must be present relative to the data folder at `fonts/fa-solid-900.ttf`.
  If the editor is packaged (e.g. the CI dist step), the CI workflow already copies
  `data/` into `dist/data/`, so the font will be included automatically.
- `io.Fonts->Build()` must remain after all font additions in the constructor; adding
  fonts after `Build()` is a no-op until the atlas is invalidated.
- `ImGui::SetItemTooltip` requires ImGui ≥ 1.89 — the docking branch satisfies this.

## Skills and instructions used

- CLAUDE.md: Google C++ style guide, one class per file, `cpplint` before commit,
  conventional commit messages, history file required.
- `src/editor/CLAUDE.md`: editor is leaf of dependency graph, all ImGui calls bracketed
  by `NewFrame()`/`Render()`.
