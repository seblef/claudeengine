# Slate & Teal ImGui Theme

**Date:** 2026-05-15
**Branch:** `feat/slate-teal-theme`
**Issue:** #200

## Summary

Replaced `ImGui::StyleColorsDark()` with a bespoke **Slate & Teal** theme: cool dark
slate backgrounds, subtle blue-grey borders, and a teal/cyan accent (`#2FC4B2`).

## Palette

| Role | Hex | ImVec4 |
|---|---|---|
| Window bg | `#1E2228` | `(0.118, 0.133, 0.157)` |
| Panel / child bg | `#272C35` | `(0.153, 0.173, 0.208)` |
| Titlebar / menubar | `#1A1F26` | `(0.102, 0.122, 0.149)` |
| Border | `#353C48` | `(0.208, 0.235, 0.282)` |
| **Accent (teal)** | `#2FC4B2` | `(0.184, 0.769, 0.698)` |
| Text | `#D4D8E0` | `(0.831, 0.847, 0.878)` |
| Dimmed text | `#6B737F` | `(0.420, 0.451, 0.498)` |

## Changes

### `src/editor/EditorTheme.h` / `EditorTheme.cpp` (new)

- Single free function `editor::ApplySlateAndTealTheme()`.
- Sets all `ImGuiStyle` metrics: rounding (6/4/4/6/4/4/4 px), padding, spacing, scroll-
  bar size, border sizes.
- Sets every `ImGuiCol_*` entry explicitly — independent of any base style call.
- Palette constants are file-local `constexpr ImVec4` values; alpha variants generated
  via `WithAlpha()` helper to keep the table readable.

### `src/editor/EditorSystem.cpp`

- `ImGui::StyleColorsDark()` → `ApplySlateAndTealTheme()`.
- Added `#include "editor/EditorTheme.h"`.

### `src/editor/EditorToolbar.cpp`

- `kActiveColour` updated from the ImGui default blue to `(0.184, 0.769, 0.698, 1.0)`.

### `src/editor/ObjectsPanel.cpp`

- `kSelectedColor` updated from the ImGui default blue to teal @ 0.35 alpha.

### `src/editor/CMakeLists.txt`

- `EditorTheme.cpp` added to the `editor` static library sources.

## Decisions

**Why a dedicated `EditorTheme.cpp`?**  Keeping all color/style assignments in one place
makes future theme iteration fast — no hunting across multiple files. It also makes
`EditorSystem` constructor shorter and easier to read.

**Why explicit `constexpr ImVec4` palette constants?**  The assignments are self-docu-
menting (`c[ImGuiCol_Button] = kPanelBg`) without relying on inline magic numbers. The
`WithAlpha()` helper avoids repeating `.x/.y/.z` and keeps alpha variants at a glance.

**Why set every `ImGuiCol_*` entry?**  Relying on a base style call (like `StyleColorsDark`)
as a starting point means the theme breaks silently when ImGui adds new color slots. An
exhaustive list is immune to such regressions.

## Output to keep in mind

- The teal accent value `(0.184f, 0.769f, 0.698f)` is now the canonical accent in the
  editor. Any new panel that needs a highlight color should use this value rather than
  the ImGui default blue.
- `ApplySlateAndTealTheme()` must be called after `ImGui::CreateContext()` and before
  the first `ImGui::NewFrame()`.

## Skills and instructions used

- CLAUDE.md: Google C++ style, one class per file, `cpplint`, conventional commits,
  history file.
- `src/editor/CLAUDE.md`: editor is the dependency-graph leaf; all ImGui calls
  bracketed by `NewFrame()`/`Render()`.
