# ClaudeEngine

A multi-platform 3D shoot'em up game engine written in modern C++.

## Prerequisites

- CMake 3.21+
- A C++20 compiler (GCC 10+, Clang 12+, MSVC 2022+)
- Git (for dependency fetching)

[loguru](https://github.com/emilk/loguru) is fetched automatically at configure time — no manual download needed.

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
```

Logs are written to both stderr and `claude_engine.log` in the working directory.

## Build profiles

| Profile | Optimisation | Debug symbols | Logging |
|---|---|---|---|
| `dev` (default) | Off (`-O0`) | Yes | Verbose |
| `stable` | Aggressive (`-O3`, LTO) | No | Minimal |
