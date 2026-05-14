# CLAUDE.md — editor module

## Role

The `editor` module is the top-level application layer for the game editor. It owns
the editor loop, ImGui lifecycle, scene management, and all editor UI panels. It
reuses `game`, `renderer`, `gldevices`, `abstract`, and `core` but adds no
game-runtime features to them.

## Dependency graph

```
editor_app → editor → game → renderer → abstract → core
                    ↘ gldevices ↗
```

**CRITICAL invariant**: editor code must **never** be imported by `game`,
`renderer`, `gldevices`, `abstract`, or `core`. The `editor` module is the
leaf of the dependency graph.

## Module structure

| File(s) | Responsibility |
|---|---|
| `EditorSystem` | Singleton owning the editor loop, ImGui init/shutdown, and top-level subsystem wiring |

## Key third-party libraries

| Library | Purpose | CMake target |
|---|---|---|
| ImGui (docking) | All editor UI | `imgui` |
| ImGuizmo | Transform gizmo in the viewport | `imguizmo` |
| nfd | Native file dialogs for Import menu | `nfd` |

These are only available when the project is configured with `BUILD_EDITOR=ON`.

## Build requirement

The editor requires `libgtk-3-dev` on Linux (used by nfd for native file dialogs).
Configure with:
```
cmake -S . -B _build -DBUILD_EDITOR=ON
```

## Guidelines

Follow all rules in `src/CLAUDE.md`. Additionally:
- One class per `.h` / `.cpp` pair.
- All ImGui calls must be bracketed by `ImGui::NewFrame()` / `ImGui::Render()`.
- Editor subsystems (panels, viewport, scene) are owned by `EditorSystem` and
  created in its constructor; they must not be singletons.
