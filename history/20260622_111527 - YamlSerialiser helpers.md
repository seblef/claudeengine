# YamlSerialiser helpers — `core/YamlSerialiser.h/.cpp`

**Issue**: #708  
**Branch**: `feat/core-yaml-serialiser-708`  
**Milestone**: M1 — Play-in-Editor

## What was added

New files `src/core/YamlSerialiser.h` and `src/core/YamlSerialiser.cpp` expose
write-side YAML helpers and a path reader in `namespace core::yaml`:

| Function | Description |
|---|---|
| `WriteVec3f(out, key, v)` | Emits `key: [x, y, z]` flow sequence |
| `WriteColor(out, key, c)` | Emits `key: [r, g, b, a]` flow sequence |
| `WriteMat4(out, key, m)` | Emits `key: [f0 … f15]` 16-element flat sequence (row-major) |
| `ReadPath(node, key, base_dir)` | Reads `node[key]` as a relative path string, resolves against `base_dir`; warns and returns `{}` on missing/non-scalar |
| `WritePath(out, key, path, base_dir)` | Writes `path` relative to `base_dir`; falls back to absolute string when roots differ |

## Decisions and rationale

### Scope reduction vs. the original issue

The original issue proposed `ReadVec3f`, `ReadColor`, `ReadQuat`/`WriteQuat`
alongside the write helpers. Two GitHub comments (from the author) revised the
scope before implementation:

1. **`ReadVec3f` / `ReadColor` dropped** — they are direct duplicates of
   `core::ParseVec3` / `core::ParseColor` already in `YamlUtils.h`. Callers
   use those directly.

2. **`ReadQuat` / `WriteQuat` dropped** — `Quatf` does not yet exist in `core/`.
   They will be added to this file when `Quatf` is introduced.

### `WriteMat4` rounds-trips with `ParseMat4`

`YamlUtils::ParseMat4` reads a 16-element flat sequence in row-major order;
`WriteMat4` writes in the same format, so any existing map file that was
serialised with one can be deserialised with the other.

### `WritePath` uses `lexically_relative`

`std::filesystem::path::lexically_relative` is a pure string operation
(no filesystem access) and guarantees a relative path when both paths share a
root. If they don't share a root (different drives on Windows, or an absolute
path that is not under `base_dir`) it returns an empty path; the function then
falls back to the absolute path string rather than silently emitting nothing.

### All Write* helpers are map-context helpers

They emit `YAML::Key << key << YAML::Value << …` which is only valid inside an
active YAML map. This precondition is documented in the header; callers are
descriptor `SaveToYaml` implementations that always open a `BeginMap` first.

## Files changed

| File | Change |
|---|---|
| `src/core/YamlSerialiser.h` | New — public API |
| `src/core/YamlSerialiser.cpp` | New — implementations |
| `src/core/CMakeLists.txt` | Added `YamlSerialiser.cpp` to `core` library |
| `tests/core/YamlSerialiserTest.cpp` | New — 11 GTest cases |
| `tests/core/CMakeLists.txt` | Added `YamlSerialiserTest.cpp` |

## Output to keep in mind

- `Quatf` does not exist in `core/` yet. When it is added, `ReadQuat` /
  `WriteQuat` should be appended to `YamlSerialiser.h/.cpp` (no new file
  needed).
- The YAML format for each type is fixed by the existing `ParseVec3` /
  `ParseColor` / `ParseMat4` readers; new resource descriptors must use the
  same sequences to remain round-trip-safe.

## Skills / CLAUDE.md instructions applied

- `src/CLAUDE.md` — one class/utility per `.h`/`.cpp` file; Google C++ style;
  `#include` paths relative to `src/`; new module listed in
  `src/core/CMakeLists.txt`
- Root `CLAUDE.md` — history MarkDown required; conventional commits; PR to
  `dev`.
