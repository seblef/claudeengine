# Particle Emitter Enable/Disable — Issue #583

## Summary

Added the ability to enable or disable particle emission on a per-sub-system basis,
without destroying existing live particles. A disabled emitter stops spawning new
particles; any already-alive particles finish their lifetime normally.

## Changes

### `src/particles/ParticleSubSystemDesc.h`
Added `bool enabled = true;` to `ParticleSubSystemDesc`. Defaults to `true` so all
existing `.particles.yaml` files without the key continue to work unchanged.

### `src/particles/ParticleEmitter.cpp`
`Update()` now gates the spawn phase on `desc_.enabled`:
```cpp
bool emitting = desc_.enabled && (desc_.looping || elapsed_ <= desc_.duration);
```
Live particles still integrate and expire regardless of the flag.

### `src/particles/ParticleSystemTemplate.cpp`
`ParseSubSystem()` reads the optional `enabled` YAML key (defaults to `true`).

### `src/editor/ParticleEditorWindow.cpp`
Three touch-points:
- **`SerializeToFile()`** — emits `enabled:` for every sub-system, keeping the
  file format round-trip safe.
- **`RenderSubSystemList()`** — each row now has an inline checkbox before the
  sub-system name. Greys out the label when disabled for immediate visual feedback.
  Checking/unchecking marks the effect as unsaved and triggers a preview rebuild.
- **`RenderSubSystemProperties()`** — an `Enabled` checkbox is shown just below
  the Name field so users can also toggle it from the property panel.

## Decisions

- **Emit stops, live particles continue**: disabling stops spawning only. This
  matches the expected mental model (stop the fire, let smoke fade naturally).
- **Default `true`**: backward-compatible; no migration of existing data files.
- **Dual toggle surface**: both the list checkbox and the property-panel checkbox
  write the same field — no state duplication, both call `unsaved_ = preview_dirty_ = true`.
- **Grey label in list**: uses `ImGuiCol_TextDisabled` so disabled sub-systems are
  visually distinct at a glance without requiring extra icons or columns.

## Skills / CLAUDE.md rules used

- `src/CLAUDE.md`: one class per file, Google C++ style, `src/` include root.
- `src/particles/CLAUDE.md`: dependency graph, one struct per header.
- `src/editor/CLAUDE.md`: panels must be pure UI; GUI vs. edition logic separation.
- Global CLAUDE.md: conventional commits, history file, cpplint before commit.
