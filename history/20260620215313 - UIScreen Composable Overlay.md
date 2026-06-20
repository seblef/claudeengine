# UIScreen Composable Overlay (issue #695)

## Changes

### New files

| File | Purpose |
|---|---|
| `src/ui/UIScreen.h` | Abstract base class: `Build()` hook, `Show()`/`Hide()`, owns sprite/text vectors |
| `src/ui/UIScreen.cpp` | Registers/unregisters with `UIRenderer::Instance()` on `Show()`/`Hide()` |
| `src/ui/HUDScreen.h` | Concrete HUD: crosshair sprite, health/ammo UIText labels |
| `src/ui/HUDScreen.cpp` | Loads crosshair texture + font atlas in `Build()`; `SetHealth`/`SetAmmo` update text |
| `src/ui/LoadingScreen.h` | Full-screen loading overlay: black background, "Loading..." text, progress bar |
| `src/ui/LoadingScreen.cpp` | Uses a 1×1 white `RawTexture` tinted to achieve solid-colour quads |

### Modified files

| File | Change |
|---|---|
| `src/ui/UISprite.h` | Added `RawTexture*` constructor overload and `GetRawTexture()` getter |
| `src/ui/UISprite.cpp` | Implemented the `RawTexture*` constructor |
| `src/ui/UIRenderer.cpp` | `RenderSprites()` now binds `GetTexture()` or `GetRawTexture()` depending on which is set |
| `src/ui/CMakeLists.txt` | Added `HUDScreen.cpp`, `LoadingScreen.cpp`, `UIScreen.cpp` |
| `src/game/GameSystem.h` | Added `unique_ptr<HUDScreen>`, `unique_ptr<LoadingScreen>`, and getter methods |
| `src/game/GameSystem.cpp` | Builds both screens in the constructor |
| `src/game/CMakeLists.txt` | Added `ui` to game's link libraries |

## Design decisions

### Solid-colour sprites via `RawTexture*`
`UISprite` previously only accepted `abstract::Texture*` (file-loaded, ref-counted). The
`LoadingScreen` needs solid black/white quads with no asset file. Rather than adding a dummy
PNG asset, `UISprite` was extended with an overloaded constructor for `abstract::RawTexture*`.
`UIRenderer::RenderSprites()` binds whichever pointer is non-null. The 1×1 white RGBA tileable
texture is created once per `LoadingScreen` via `CreateTileableTexture` and stored as a
`unique_ptr<RawTexture>` in the screen.

### `visible_` guard in `UIScreen`
`Show()` and `Hide()` guard against double-registration with a `bool visible_` flag — calling
either twice is a no-op. The destructor calls `Hide()` if still visible, preventing dangling
pointers in `UIRenderer`'s lists after `UIScreen` destruction.

### Font path
Both `HUDScreen` and `LoadingScreen` load `fonts/fa-solid-900.ttf` (the only font in
`data/fonts/`). This is FontAwesome (icon glyphs), which will not render ASCII text
correctly. A proper text font (e.g. a variable-weight sans-serif TTF) should be added to
`data/fonts/` and passed as a constructor argument or a configuration value in future work.
The `Build()` signature takes `abstract::VideoDevice*` which provides `GetWidth()`/`GetHeight()`
for resolution-relative layout.

### `game → ui` dependency
`game/CMakeLists.txt` now links publicly to `ui`. The `ui` module depends on `abstract`
(already in game's transitive closure), so no circular dependency is introduced. The `ui`
CLAUDE.md explicitly states `UIRenderer` is consumed by the game system or another caller,
not by `renderer::Renderer`.

### Crosshair texture
`HUDScreen::Build()` loads `"ui/crosshair.png"` via `video->CreateTexture()`. If the file
is absent the sprite is simply skipped (null check). The asset file needs to be created and
placed in `data/textures/ui/crosshair.png` before shipping.

### Progress bar
`LoadingScreen::SetProgress(float fraction)` clamps `fraction` to `[0, 1]` and scales the
bar's width to `fraction * screen_width`. The bar height is a fixed 8 px; Y is at 85 % of
the screen height.

## Key things to remember
- `UIScreen::~UIScreen()` always calls `Hide()` — no dangling pointers on destruction
- `UIScreen::visible_` prevents double Add/Remove in UIRenderer
- `UISprite` now has two texture paths: `texture_` (file-based) and `raw_texture_` (raw CPU upload)
- `ui` must be in `game`'s link libraries for `HUDScreen`/`LoadingScreen` includes
- Crosshair asset path: `data/textures/ui/crosshair.png` (needs to be created)
- Font used: `data/fonts/fa-solid-900.ttf` (icon font — swap for a text font when available)

## Skills / CLAUDE.md instructions used
- `impl-issue` skill
- `src/CLAUDE.md`: one class per `.cpp`/`.h`, Google C++ style, no unnecessary comments
- `src/ui/CLAUDE.md`: one class per file, no platform/GL includes, no dependency on `renderer/`
- `src/game/CLAUDE.md`: dependency graph `game → renderer → abstract`, one class per file
- Root-level `CLAUDE.md`: conventional commits, history file, cpplint before commit
