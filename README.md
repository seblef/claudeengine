# ClaudeEngine

A multi-platform 3D shoot'em up game engine written in modern C++.

## Prerequisites

- CMake 3.21+
- A C++20 compiler (GCC 10+, Clang 12+, MSVC 2022+)
- Git (for dependency fetching)

[loguru](https://github.com/emilk/loguru) is fetched automatically at configure time — no manual download needed.

## Dev tools

The following tools are required for contributing. They mirror the CI quality gates, so installing them locally catches issues before push.

### cpplint

Enforces the Google C++ Style Guide.

```bash
# Linux / macOS
pip install cpplint

# Windows (same)
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

```bash
# Clone the repository
git clone <repo-url>
cd claudeengine

# Configure (dev profile — debug symbols, verbose logging)
cmake -S . -B _build -DBUILD_PROFILE=dev

# Configure (stable profile — optimised, minimal binary)
cmake -S . -B _build -DBUILD_PROFILE=stable

# Build
cmake --build _build
```

The binary is produced at `build/claude_engine` (Linux/macOS) or `build/claude_engine.exe` (Windows).

## Run

```bash
./build/claude_engine

# Override log verbosity at runtime (0 = INFO, 9 = max)
./build/claude_engine -v 9

# Set the data folder (defaults to the current working directory)
./build/claude_engine --data-path /path/to/data

# Typical dev setup: binary in build/, data in the repository root
./build/claude_engine --data-path ../data
```

Logs are written to both stderr and `claude_engine.log` in the working directory.

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
