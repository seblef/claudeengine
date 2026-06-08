# PostProcessConfig — post-processing feature flags

## Summary

Added `core::PostProcessConfig`, a new config class that reads a `post_process` section from `config.yaml` and exposes boolean toggles for bloom and eye adaptation. The class follows the existing `GraphicsConfig` / `ShadowConfig` pattern exactly.

## Changes

| File | Change |
|---|---|
| `src/core/PostProcessConfig.h` | New class declaration |
| `src/core/PostProcessConfig.cpp` | `Parse()` implementation with INFO logging |
| `src/core/AppConfig.h` | Added `#include`, static member, and `GetPostProcess()` accessor |
| `src/core/AppConfig.cpp` | Static member definition; parse `post_process` section in `Init()` |
| `src/core/CMakeLists.txt` | Added `PostProcessConfig.cpp` to the `core` library |
| `data/config.yaml` | Added `post_process: { bloom: true, eye_adaptation: true }` |

## Decisions

- **Defaults to `true`**: Both flags default to `true` so existing deployments without the config section are unaffected (opt-out rather than opt-in).
- **Parse style mirrors `GraphicsConfig`**: Inline guard per key (`if (node["key"])`), single `LOG_F(INFO, ...)` at the end — no deviation from established style.
- **One class per file**: `GraphicsConfig::Parse` is defined in `AppConfig.cpp` (pre-existing pattern); `PostProcessConfig` is kept in its own `.h`/`.cpp` pair as required.

## Output to keep in mind

- `AppConfig::GetPostProcess()` is available engine-wide; renderer code can call `AppConfig::GetPostProcess().IsBloomEnabled()` / `IsEyeAdaptationEnabled()` to gate post-processing passes and select shader paths.
- No renderer changes in this PR — this is a pure config layer addition.

## Skills / instructions consulted

- `CLAUDE.md` (root): Git workflow, conventional commits, history file requirement
- `src/CLAUDE.md`: One class per file rule, Google C++ style, include root convention
- `impl-issue` skill: branch from `dev`, PR to `dev`, close issue with `Close #410`
