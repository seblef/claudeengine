# CLAUDE.md — ui module

## Role

The `ui` module owns all 2D screen-space overlay elements: sprite quads, text
strings, font atlases, and the rendering backend that draws them. It is
**independent of the 3D renderer** — `renderer` is not in its dependency chain
and must not be added.

## Dependency graph

```
game → ui → abstract → core
```

`UIRenderer` is a Singleton consumed directly by the game system (or any other
caller), not by `renderer::Renderer`. The caller is responsible for invoking
`UIRenderer::Instance().Render()` after the 3D frame is complete.

## Module structure

| File(s) | Responsibility |
|---|---|
| `GlyphInfo` (in `FontAtlas.h`) | Header-only POD: per-glyph UV rect, bearing, advance, and bitmap size |
| `FontAtlas` | Loads a TTF file via stb_truetype, bakes ASCII printable range into an R8 GPU atlas texture, and exposes per-glyph metrics |
| `UISprite` | Pure data: a textured quad in screen-pixel space with a multiplicative tint and a draw layer |
| `UIText` | Pure data: a string drawn at a baseline position using a `FontAtlas`, with a color and a draw layer |
| `UIRenderer` | Singleton GPU renderer: owns sprite/text lists, builds per-frame quad geometry, and renders sprites then texts into the currently bound framebuffer |

## Key invariants

- `UISprite` and `UIText` are **pure data** — they hold no GPU resources and
  issue no draw calls. Only `UIRenderer` touches the GPU.
- `UIRenderer` owns the registered element lists via `Add/RemoveUISprite` and
  `Add/RemoveUIText`. Pointers must remain valid on every subsequent `Render()`
  call.
- **CB slot 4** is reserved for the orthographic projection matrix. No other
  pass in the engine may use slot 4.
- Render order is sprites sorted by `layer_`, then texts sorted by `layer_`.
  Elements with a higher layer value render on top.
- `UIRenderer::Render()` saves and restores depth-test and blend state so it
  leaves the pipeline in its default configuration.
- `UIRenderer::OnResize()` must be called whenever the window resolution
  changes to recompute the ortho matrix.

## Guidelines

Follow all rules in `src/CLAUDE.md`. Additionally:
- One class per `.h` / `.cpp` pair.
- `GlyphInfo` is header-only (pure POD inside `FontAtlas.h`); all other classes
  have a corresponding `.cpp`.
- Do not include platform or OpenGL headers directly; go through `abstract/`.
- Do not add a dependency on `renderer/` — the decoupling is intentional.
