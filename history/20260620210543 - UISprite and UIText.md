# UISprite and UIText screen-space element types

**Issue**: #693
**PR**: #701
**Branch**: `feat/ui-sprite-text-693`
**Date**: 2026-06-20

## What was implemented

Two pure data types that describe screen-space UI elements for consumption by `UIRenderer`:

### `UISprite` (`src/ui/UISprite.h/.cpp`)
- Stores a non-owning `abstract::Texture*` pointer
- Rect (`core::Vec4f`) encodes `(x, y, w, h)` in pixels — top-left origin
- Tint (`core::Vec4f`) as `(r, g, b, a)`, defaulting to `(1,1,1,1)`
- `SetPosition` / `SetSize` / `SetTint` mutate in-place with no heap allocation

### `UIText` (`src/ui/UIText.h/.cpp`)
- Stores a non-owning `FontAtlas*` pointer
- Text stored as `std::string` — `SetText` reuses capacity on repeated calls
- Position as `core::Vec2f` (baseline origin in pixels)
- Scale factor `float size_` relative to atlas `pixel_height` (1.0 = native)
- Color as `core::Vec4f`, defaulting to `(1,1,1,1)`

## Key decisions

### All getters inline in header
Simple field reads were left as `inline` in the header rather than in `.cpp`, consistent with `Vec2f`/`Vec4f` pattern in `core/`. There is no compilation-time concern here as these are trivial accessors.

### `const char*` interface, `std::string` storage
The issue explicitly requested `const char*` in the public API and `std::string` for storage. `SetText(const char*)` assigns into the `std::string` — subsequent calls with the same or shorter string reuse the buffer (no alloc after the first).

### `Vec4f` for rect
Using `Vec4f(x, y, w, h)` keeps the rect as a single value that `UIRenderer` can pass directly to a shader uniform. `SetPosition` and `SetSize` mutate `x/y` and `z/w` directly.

## Files changed

| File | Change |
|---|---|
| `src/ui/UISprite.h` | New |
| `src/ui/UISprite.cpp` | New |
| `src/ui/UIText.h` | New |
| `src/ui/UIText.cpp` | New |
| `src/ui/CMakeLists.txt` | Added `UISprite.cpp`, `UIText.cpp` |

## Skills / instructions used

- `impl-issue` skill
- `src/CLAUDE.md`: one class per `.h`/`.cpp`, include root is `src/`, Google C++ style
- Root `CLAUDE.md`: conventional commits, history file, cpplint before commit, PR to `dev`

## Output to keep in mind for next features

- `UIRenderer` (next issue) will iterate `std::vector<UISprite>` and `std::vector<UIText>` each frame — both types are cheap to copy (pointer + 2× Vec4f for UISprite; pointer + string + Vec2f + float + Vec4f for UIText)
- `GetRect()` returns `(x, y, w, h)` — `UIRenderer` should document this layout clearly when it maps to a vertex buffer
- `UIText::GetText()` returns `c_str()` — valid as long as the `UIText` lives and `SetText` is not called concurrently
