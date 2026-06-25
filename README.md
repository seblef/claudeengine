# Wreckoning

A multi-platform 3D vehicular combat game inspired by Carmageddon. Drive a heavily armed car, wreck opponents, and earn time extensions through carnage.

**Win conditions** (first met wins):
- Complete all race checkpoints in order
- Wreck every opponent car
- Run over every pedestrian on the map

## Current state

The engine, editor, and core gameplay loop are functional. What works today:

| System | Status |
|---|---|
| Deferred renderer — GBuffer, shadow maps (CSM + omni), bloom, eye adaptation | Working |
| Terrain with heightmap, foliage, and trees | Working |
| GPU particle systems | Working |
| 3D positional audio | Working |
| Physics — rigid bodies, static meshes, raycasting, vehicle constraint | Working |
| Driveable player vehicle with chase camera | Working |
| Road splines conforming to terrain heightmap | Working |
| In-game world editor (selection, transform, material, road tools) | Working |
| Play-in-editor (F5 to drive the current map, Escape to return to editing) | Working |

AI opponents, pedestrians, race rules, and power-ups are on the roadmap — see `WRECKONING.md` for the full design and milestones.

## Prerequisites

### Linux system libraries

```bash
# Game engine (required)
sudo apt-get install \
    libgl1-mesa-dev libglu1-mesa-dev \
    libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev

# Editor (required only when building with -DBUILD_EDITOR=ON)
sudo apt-get install libgtk-3-dev

# Optional: TIFF texture support
sudo apt-get install libtiff-dev
```

A C++20 compiler is required: GCC 10+, Clang 12+, or MSVC 2022+. CMake 3.21+ is also required.

### Third-party libraries (auto-fetched)

All dependencies are fetched automatically by CMake at configure time via `FetchContent` — no manual downloads needed.

| Library | Version / tag | Purpose |
|---|---|---|
| [loguru](https://github.com/emilk/loguru) | `master` | Logging |
| [GLFW](https://github.com/glfw/glfw) | `3.4` | Window and input |
| [yaml-cpp](https://github.com/jbeder/yaml-cpp) | `0.8.0` | YAML config and material files |
| [fast_obj](https://github.com/thisistherk/fast_obj) | `master` | OBJ mesh loading |
| [ufbx](https://github.com/ufbx/ufbx) | `master` | FBX mesh loading |
| [stb](https://github.com/nothings/stb) | `master` | JPEG / PNG texture loading |
| [Jolt Physics](https://github.com/jrouwe/JoltPhysics) | `v5.5.0` | Rigid-body physics simulation |
| [ImGui (docking)](https://github.com/ocornut/imgui) | `docking` | Editor UI *(editor only)* |
| [ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo) | `master` | Transform gizmo *(editor only)* |
| [nfd-extended](https://github.com/btzy/nativefiledialog-extended) | `master` | Native file dialogs *(editor only)* |

Editor-only libraries are fetched only when `-DBUILD_EDITOR=ON` is passed to CMake.

## Build

### Clone and configure

```bash
git clone <repo-url>
cd wreckoning
```

### Game

```bash
# Dev profile — debug symbols, verbose logging
cmake -S . -B _build -DBUILD_PROFILE=dev

# Stable profile — optimised, minimal binary
cmake -S . -B _build -DBUILD_PROFILE=stable

cmake --build _build
```

The binary is produced at `build/wreckoning`.

### Editor

The editor requires `libgtk-3-dev` on Linux (native file dialogs). Enable it with the `BUILD_EDITOR` flag:

```bash
cmake -S . -B _build -DBUILD_PROFILE=dev -DBUILD_EDITOR=ON
cmake --build _build
```

This produces a second binary at `build/wreckoning_editor`. Both binaries share the same `data/` directory.

## Run

### Game

```bash
./build/wreckoning

# Set the data folder (defaults to the current working directory)
./build/wreckoning --data-path /path/to/data

# Override log verbosity at runtime (0 = INFO, 9 = max)
./build/wreckoning -v 9
```

### Editor

```bash
./build/wreckoning_editor

# Same flags as the game engine
./build/wreckoning_editor --data-path /path/to/data
```

Logs are written to both stderr and a `.log` file in the working directory.

### Command-line arguments

| Argument | Description | Default |
|---|---|---|
| `--map <path>` | Map file to load on startup (`.map.yaml`); also accepted as a bare positional argument | *(none — demo scene)* |
| `--vehicle <path>` | Vehicle descriptor to spawn at the map's Player Start (e.g. `vehicles/car.vehicle.yaml`); requires `--map` | *(none — FPS camera)* |
| `--data-path <path>` | Root directory for engine data files (maps, textures, shaders, …) | Current working directory (`.`) |
| `-v <level>` | Log verbosity (0 = INFO, 9 = max debug) | `0` |

Example — load a map and drive a vehicle:

```bash
./build/wreckoning --map data/maps/demo.map.yaml --vehicle vehicles/car.vehicle.yaml
```

## Build profiles

| Profile | Optimisation | Debug symbols | Logging |
|---|---|---|---|
| `dev` (default) | Off (`-O0`) | Yes | Verbose |
| `stable` | Aggressive (`-O3`, LTO) | No | Minimal |

## CMake options

| Option | Default | Description |
|---|---|---|
| `BUILD_PROFILE` | `dev` | Build profile: `dev` or `stable` |
| `BUILD_EDITOR` | `OFF` | Build the editor executable (`wreckoning_editor`) |

## Playing the game

### Driving controls

| Key | Action |
|---|---|
| `W` / `↑` | Throttle |
| `S` / `↓` | Brake / reverse |
| `A` / `←` | Steer left |
| `D` / `→` | Steer right |
| `Space` | Handbrake |

### Debug / developer keys

| Key | Action |
|---|---|
| `0` / `Escape` | Full pipeline (disable debug overlay) |
| `1` | G-buffer — albedo |
| `2` | G-buffer — normals |
| `3` | G-buffer — specular |
| `4` | G-buffer — depth |
| `Tab` | Cycle shadow-map debug overlay |
| `P` | Toggle physics body wireframe overlay |

### FPS camera controls (walkthrough mode)

These apply when no vehicle is active (e.g. the standalone game app without a vehicle start).

| Key | Action |
|---|---|
| `↑` / `↓` / `←` / `→` | Move forward / back / left / right |
| `Space` | Jump |
| `A` | Move up (free-fly) |
| `Z` | Move down (free-fly) |
| Mouse | Look around |

## Using the editor

Open a map via **File → Open** or from the recent maps list. The editor viewport uses an orbit camera. Press **F5** or click the **Play** button in the toolbar to drive the current map; press **F5** or **Escape** to return to editing.

### Editor camera navigation

| Input | Action |
|---|---|
| `Alt` + LMB drag | Orbit around focus point |
| RMB drag | Fly-through (WASD + Q/E to move, release RMB to stop) |
| `Shift` + RMB drag | Pan |
| Scroll wheel | Zoom in / out |
| `Numpad 1` | Front view |
| `Numpad 3` | Right view |
| `Numpad 7` | Top view |

### Editor toolbar shortcuts

| Key | Action |
|---|---|
| `Q` | Select tool |
| `C` | Camera tool |
| `W` | Translate tool |
| `E` | Rotate tool |
| `R` | Scale tool |
| `F5` | Play / Stop |
| `Escape` | Stop (play mode only) |
| `Ctrl+Z` | Undo |
| `Ctrl+Shift+Z` | Redo |
| `Ctrl+C` | Copy selected object |
| `Ctrl+V` | Paste |
| `Ctrl+S` | Save map |

### Play-in-editor workflow

1. Place a **Player Start** object in the scene (toolbar → flag icon).
2. Press **F5** — the map auto-saves and you immediately drive it.
3. Press **F5** or **Escape** to stop. The editor restores the scene exactly as it was.

> A vehicle descriptor must exist at `data/vehicles/<name>.vehicle.yaml` and be configured in the scene for Play mode to activate.

### Configuration

The engine reads `data/config.yaml` on startup. Copy `data/config.yaml.dist` to `data/config.yaml` to create your local configuration. Key settings:

| Setting | Description |
|---|---|
| `graphics.width` / `graphics.height` | Window resolution |
| `graphics.windowed` | `true` for windowed, `false` for fullscreen |
| `post_process.bloom` | Enable / disable bloom |
| `post_process.eye_adaptation` | Enable / disable auto-exposure |
| `player.move_speed` | FPS camera movement speed |
| `player.mouse_sensitivity` | Mouse look sensitivity |

## Dev tools

The following tools are required for contributing. They mirror the CI quality gates.

### cpplint

Enforces the Google C++ Style Guide.

```bash
pip install cpplint
```

### cppcheck

Static analysis tool for C++.

```bash
# Ubuntu / Debian
sudo apt-get install cppcheck

# macOS
brew install cppcheck

# Windows — download the installer from https://cppcheck.sourceforge.io/
```

### Pre-commit hook

A pre-commit hook that runs both tools on staged files is provided in `.hooks/pre-commit`. Install it once after cloning:

```bash
cp .hooks/pre-commit .git/hooks/pre-commit
```

The hook runs automatically on every `git commit` and only checks staged files.
