# ClaudeEngine

A multi-platform 3D shoot'em up game engine written in modern C++.

## Prerequisites

- CMake 3.21+
- A C++20 compiler (GCC 10+, Clang 12+, MSVC 2022+)
- Git (for dependency fetching)

### Linux system libraries

Install the following packages before configuring:

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
| [ImGui (docking)](https://github.com/ocornut/imgui) | `docking` | Editor UI *(editor only)* |
| [ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo) | `master` | Transform gizmo *(editor only)* |
| [nfd-extended](https://github.com/btzy/nativefiledialog-extended) | `master` | Native file dialogs *(editor only)* |

Editor-only libraries are fetched only when `-DBUILD_EDITOR=ON` is passed to CMake.

## Dev tools

The following tools are required for contributing. They mirror the CI quality gates, so installing them locally catches issues before push.

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

The hook runs automatically on every `git commit`. It only checks files that are staged, so it adds negligible overhead.

## Build

### Game engine

```bash
git clone <repo-url>
cd claudeengine

# Dev profile — debug symbols, verbose logging
cmake -S . -B _build -DBUILD_PROFILE=dev

# Stable profile — optimised, minimal binary
cmake -S . -B _build -DBUILD_PROFILE=stable

cmake --build _build
```

The binary is produced at `build/claude_engine`.

### Editor

The editor requires `libgtk-3-dev` on Linux (native file dialogs). Enable it with the `BUILD_EDITOR` flag:

```bash
cmake -S . -B _build -DBUILD_PROFILE=dev -DBUILD_EDITOR=ON
cmake --build _build
```

This produces a second binary at `build/claude_editor` alongside the game executable. Both binaries share the same `data/` directory.

## Run

### Game engine

```bash
./build/claude_engine

# Set the data folder (defaults to the current working directory)
./build/claude_engine --data-path /path/to/data

# Override log verbosity at runtime (0 = INFO, 9 = max)
./build/claude_engine -v 9
```

### Editor

```bash
./build/claude_editor

# Same flags as the game engine
./build/claude_editor --data-path /path/to/data
```

Logs are written to both stderr and a `.log` file in the working directory.

### Command-line arguments

| Argument | Description | Default |
|---|---|---|
| `--data-path <path>` | Root directory for engine data files (maps, textures, shaders, …) | Current working directory (`.`) |
| `-v <level>` | Log verbosity (0 = INFO, 9 = max debug) | `0` |

## Build profiles

| Profile | Optimisation | Debug symbols | Logging |
|---|---|---|---|
| `dev` (default) | Off (`-O0`) | Yes | Verbose |
| `stable` | Aggressive (`-O3`, LTO) | No | Minimal |

## CMake options

| Option | Default | Description |
|---|---|---|
| `BUILD_PROFILE` | `dev` | Build profile: `dev` or `stable` |
| `BUILD_EDITOR` | `OFF` | Build the editor executable (`claude_editor`) |
