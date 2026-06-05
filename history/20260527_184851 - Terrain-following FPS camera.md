# Terrain-following FPS camera — issue #317

## Changes

### `src/game/FPSCameraController.h`
- Forward-declared `namespace terrain { class TerrainData; }` to avoid a full include in the header.
- Added public `void SetTerrain(const terrain::TerrainData* terrain)` method.
- Added `static constexpr float kPlayerHeight = 1.8f` (eye height above ground in metres).
- Added private `const terrain::TerrainData* terrain_ = nullptr` member.

### `src/game/FPSCameraController.cpp`
- Added `#include "terrain/TerrainData.h"`.
- Implemented `SetTerrain()` (one-liner store).
- In `Update()`, after all movement deltas are applied, added the minimum-Y clamp:
  ```cpp
  if (terrain_) {
      const float ground = terrain_->GetHeight(position_.x, position_.z);
      if (position_.y < ground + kPlayerHeight)
          position_.y = ground + kPlayerHeight;
  }
  ```

### `src/app/main.cpp`
- Added `#include "game/GameTerrain.h"`.
- Added `game::GameTerrain* map_terrain = nullptr` alongside the other map-scoped pointers.
- In the map-object scan loop, detect the first `kTerrain` object via `GetType()` and cast it to `GameTerrain*`.
- After `game.SetCameraController(&controller)`, call `controller.SetTerrain(map_terrain ? &map_terrain->GetData() : nullptr)`.

## Decisions

- **Forward declaration in header**: `TerrainData` is only stored as a raw pointer; a forward declaration keeps compile times low and avoids coupling the `game` module header to the `terrain` module header for downstream consumers.
- **Raw pointer (non-owning)**: the terrain is owned by `GameTerrain` which lives in `map_objects`; the controller merely observes it. Lifetime is guaranteed: the controller is destroyed before `map_objects` goes out of scope in `main`.
- **Height clamp only (no snap on set)**: `SetTerrain` does not reposition the camera immediately; the clamp runs on the next `Update()` call, which is consistent with the jump-landing detection reuse noted in the issue.
- **No guard for out-of-bounds**: `TerrainData::GetHeight()` already clamps to heightmap boundaries — no additional guard added per issue notes.

## Output to keep in mind

- `kPlayerHeight = 1.8f` is used by the FPS controller and will be reused by jump landing detection (future issue).
- The terrain pointer is set once after map load and cleared to `nullptr` when no terrain exists; the controller gracefully falls back to free-fly mode.

## Skills / CLAUDE.md rules applied

- `src/CLAUDE.md`: one class per file, Google C++ style, include root is `src/`.
- `src/game/CLAUDE.md`: no platform/OpenGL headers; one class per `.h/.cpp` pair.
- `src/terrain/CLAUDE.md`: `terrain` module has no upward dependencies — the `game` module depends on terrain, not the reverse.
- Conventional commit message: `feat(game): terrain-following FPS camera (#317)`.
- PR targets `dev`; includes `Close #317`.
