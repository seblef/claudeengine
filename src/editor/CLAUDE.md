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
- **Value members in `EditorWindow.h` require a full `#include`**, not a
  forward declaration. A forward declaration is only sufficient for pointer and
  reference members. If you add a by-value member of type `Foo`, include
  `editor/Foo.h` in the header; a `class Foo;` forward-declare will cause an
  "incomplete type" error.
- **Prefer `std::find_if` over raw index loops.** The pre-commit cppcheck hook
  flags raw loops with a `useStlAlgorithm` warning. Use `std::find_if` (plus
  `std::distance` when the index is needed) even for simple linear searches.

## GUI vs. edition logic separation (CRITICAL)

Panel classes (files named `*Panel`, `*Window`) must be **pure UI**:
they render controls, read user input, and call setters — nothing else.

All editing algorithms (heightmap mutation, splatmap painting, object
placement, undo/redo command construction) must live in a dedicated
class: an `EditorToolBase` subclass (for viewport-driven, per-frame
operations) or a standalone command/utility class (for button-triggered
one-shot operations).

Concretely:
- A panel **may** own a tool via `unique_ptr<MyTool>`, create it in
  `SetContext()`, and expose it via `GetTool()` so the viewport can
  activate it.
- A panel **may** read tool state through getters and write it through
  setters (e.g. `tool_->SetRadius(r)`).
- A panel **must not** implement brush math, geometry algorithms, GPU
  uploads, or command pushes inline in its `Render*()` methods.

The split has two benefits: the editing logic is testable in isolation,
and the panel stays readable as a pure description of the UI layout.
