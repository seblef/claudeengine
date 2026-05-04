# Data Folder Path as Command-Line Argument (Issue #30)

## Changes

### `src/core/Config.h` (created)

- Declares `core::Config` — a static-only class (no instances) for engine-wide configuration
- `static void Init(int argc, char* argv[])` — parses `--data-path <path>` from the command line
- `static const std::filesystem::path& GetDataFolder()` — returns the configured path

### `src/core/Config.cpp` (created)

- `data_folder_` initialised to `"."` (current directory) as the default
- `Init` iterates argv looking for `--data-path`; stops on first match
- Returns early rather than checking all args after a match is found

### `src/core/CMakeLists.txt` (modified)

- Added `Config.cpp` to the `core` static library source list

### `src/app/main.cpp` (modified)

- Added `#include "core/Config.h"`
- Calls `core::Config::Init(argc, argv)` immediately after `Logger::Init`
- Logs the effective data folder at startup: `"Data folder: <path>"`

### `README.md` (modified)

- Extended the **Run** section with `--data-path` usage examples
- Added a **Command-line arguments** reference table

---

## Decisions and rationale

### Static class, not a Singleton
`Config` holds values parsed once at startup and read many times thereafter. A static class (all members and methods static, constructor deleted) is simpler than a Singleton — no `Instance()` call needed, and there is no meaningful polymorphism or injection scenario.

### `std::filesystem::path` as the return type
Callers that open files will need `path` objects anyway; returning the path directly avoids repeated conversions from `std::string`. The include of `<filesystem>` in the header is acceptable given every consumer of this class will deal with paths.

### Default to `"."` (not `std::filesystem::current_path()`)
`"."` is resolved lazily by the OS at the point a file is opened. Calling `current_path()` at init time would snapshot the working directory, which is identical in practice but less portable when unit tests change the working directory. Using `"."` keeps the default transparent.

### Both Logger and Config receive the full `(argc, argv)`
loguru extracts `-v <level>` and ignores everything else. Config extracts `--data-path` and ignores everything else. No argument is consumed/removed, so both parsers see the complete argv safely.

### Dev usage documented in README
The issue specifies that during development the data folder is `../data` relative to the `build/` directory. This is a runtime convention rather than a compile-time default, so it is documented in README rather than hard-coded.

---

## Output to keep in mind

- `core::Config::Init()` must be called before any module calls `GetDataFolder()`; the natural place is right after `Logger::Init()` in `main()`.
- `GetDataFolder()` returns a `const std::filesystem::path&` to the static member — safe to store as a reference only for the duration of the program.
- Dev invocation: `./build/claude_engine --data-path ../data`

## Skills and guidance followed

- `src/CLAUDE.md`: one class per header, project-relative includes, Google C++ style
- Root `CLAUDE.md`: branch/PR workflow, history file, conventional commits
