# GamePlayerStart Scene Object — Issue #315

## Summary

Added a `GamePlayerStart` scene object that marks where the FPS camera is
placed when a map is loaded in game mode.

## Files changed

| File | Change |
|---|---|
| `src/game/GamePlayerStart.h/.cpp` | New class |
| `src/game/GameObjectType.h` | Added `kPlayerStart` enum value |
| `src/game/GameObjectVisitor.h` | Added `Visit(GamePlayerStart&)` pure virtual |
| `src/game/MapLoader.cpp` | Parses `type: player_start` objects |
| `src/game/CMakeLists.txt` | Added `GamePlayerStart.cpp` |
| `src/editor/MapSerializer.h/.cpp` | Serialises `GamePlayerStart` as `type: player_start` |
| `src/app/main.cpp` | Scans map objects for player start; sets FPS controller position |

## Design decisions

**No `GetTranslation()` on `Mat4f`** — used `operator()(row, col)` to extract
the translation column (`col=3`) directly. Adding a convenience accessor to
`Mat4f` was considered but would expand scope beyond the issue.

**Scan in `main.cpp`, not `GameSystem`** — the issue describes the scan as a
`GameSystem` update, but `GameSystem` has no knowledge of `FPSCameraController`
(which is an app-level concern). The scan was placed alongside the existing
camera-finding loop in `main.cpp`, following the same pattern.

**Player start only applied when no `GameCamera` exists in the map** — if the
map already contains an explicit `GameCamera`, the FPS controller is not used,
so the player start position would be irrelevant. This matches the issue's
intent (FPS camera placement).

**Warning when no player start** — logged only when a map is loaded (not in the
demo scene fallback path). Multiple player starts trigger a warning and use the
first.

**`OnAddedToScene` / `OnRemovedFromScene` are no-ops** — `GamePlayerStart` is
invisible at runtime; nothing to register with the renderer.

## Map YAML format

```yaml
objects:
  - name: PlayerStart
    type: player_start
    transform: [1,0,0,10, 0,1,0,5, 0,0,1,20, 0,0,0,1]
```

## Notes for future contributions

- Editor gizmo (visual marker for `GamePlayerStart`) is tracked as a separate
  issue.
- All `GameObjectVisitor` implementors must now provide `Visit(GamePlayerStart&)`.
  Currently only `MapSerializer::SerializeVisitor` implements the visitor.
