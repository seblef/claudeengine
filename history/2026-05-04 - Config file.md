# Config File Support

**Date:** 2026-05-04
**Branch:** feat/config-file
**Issue:** #37

## Summary

Added YAML configuration file support. `data/config.yaml` now drives window dimensions and fullscreen mode at startup, replacing the hardcoded `640×480` in `main.cpp`. yaml-cpp is integrated via FetchContent, a generic `LoadYamlFile` utility is added to `core`, and a structured `AppConfig` / `GraphicsConfig` class hierarchy reads and stores the parsed values.

## Changes

### New files
- `data/config.yaml` — initial config (graphics section: windowed, width, height)
- `src/core/YamlUtils.h` + `src/core/YamlUtils.cpp` — `core::LoadYamlFile()` wrapper around yaml-cpp; throws `std::runtime_error` on failure
- `src/core/AppConfig.h` + `src/core/AppConfig.cpp` — `GraphicsConfig` and `AppConfig` (multiple classes per file as required by the issue)

### Modified files

**Root `CMakeLists.txt`**
- Added yaml-cpp FetchContent block (tag `0.8.0`, before `add_subdirectory(src)`)

**`src/core/CMakeLists.txt`**
- Added `AppConfig.cpp` and `YamlUtils.cpp` to the `core` static library
- Added `yaml-cpp` to `target_link_libraries` (PUBLIC so consumers get the headers transitively)

**`src/app/main.cpp`**
- Added `#include "core/AppConfig.h"`
- Calls `core::AppConfig::Init(core::Config::GetDataFolder() / "config.yaml")` after `Config::Init()`
- Replaces hardcoded `GLDevices(640, 480, false)` with values from `AppConfig::GetGraphics()`
- Removed `TODO(#1)` comment

## Design Decisions

### `GraphicsConfig::Parse()` owns its own deserialization
Rather than having `AppConfig` reach into `GraphicsConfig` via a `friend` declaration, each section class exposes a `Parse(const YAML::Node&)` method. `AppConfig::Init()` just calls `graphics_.Parse(root["graphics"])`. Adding a new config section means adding a new class with its own `Parse()` — no changes to `AppConfig` itself.

### `YamlUtils` is generic
Returns `YAML::Node` directly so future consumers (materials, level files, etc.) can load any YAML document without coupling to `AppConfig`.

### yaml-cpp `0.8.0`
Pinned to a stable release tag, consistent with GLFW (`3.4`) and GTest (`v1.14.0`). Fetched into `external/yaml-cpp-src/` via the existing `FETCHCONTENT_BASE_DIR` setting.

### `AppConfig` is static-only
Matches the existing `Config` class pattern. Both are initialized once at startup and accessed globally through static getters.

## Next Steps / Output to Remember

- `AppConfig::Init()` must be called after `Config::Init()` (it needs the data folder path).
- If the config file is missing or malformed, `LoadYamlFile` throws `std::runtime_error` — the engine will terminate with an unhandled exception. Future work could add graceful fallback to defaults.
- New config sections (audio, input, etc.) follow the same pattern: add a `FooConfig` class with `Parse()` in `AppConfig.h/.cpp`, add a static member to `AppConfig`, and call `foo_.Parse(root["foo"])` in `Init()`.

## Skills and Instructions Used

- `impl-issue` skill
- `CLAUDE.md`: git workflow, conventional commits, history file, PR
- `src/CLAUDE.md`: Google C++ style, include from `src/`; exception for multiple config classes per file (explicitly stated in issue)
