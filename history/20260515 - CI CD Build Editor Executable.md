# CI/CD: Build and Release Editor Executable (Issue #179)

## Changes

### `.github/workflows/ci.yml`

**`dynamic-analysis` job**:
- Added `libgtk-3-dev` to apt-get install (nfd requires it on Linux). The job still only builds `core_tests`, but the dependency is present proactively.

**`build-dev` job**:
- Added `libgtk-3-dev` to apt-get install.
- Added `-DBUILD_EDITOR=ON` to the CMake configure step so `claude_editor` is built.
- Updated `upload-artifact` path to multi-path YAML block:
  ```yaml
  path: |
    build/claude_engine
    build/claude_editor
  ```

**`build-stable` job**:
- Same changes as `build-dev` (libgtk-3-dev, BUILD_EDITOR=ON, dual artifact path).

**`package` job**:
- Updated `chmod +x` to include both `dist/claude_engine` and `dist/claude_editor`. Both files land in `dist/` automatically from the artifact download (no extra cp needed).

## Decisions

- **libgtk-3-dev scope**: The issue spec says to add it to `dynamic-analysis` and `build-stable`. I also added it to `build-dev` since that job now builds the editor too.
- **Artifact path stripping**: `actions/upload-artifact@v4` strips the common path prefix from multi-file uploads. `build/claude_engine` and `build/claude_editor` share the `build/` prefix, so the artifact contains just the filenames. When `download-artifact` places them in `dist/`, no extra `cp` step is needed — the existing package workflow structure works unchanged.
- **`dynamic-analysis` without BUILD_EDITOR**: This job only builds `core_tests` and runs Valgrind. Adding `BUILD_EDITOR=ON` to its cmake configure would be wasteful (it would compile all editor code just to run memory checks on core tests). Left as-is; libgtk-3-dev is enough future-proofing.
- **Issue spec "dynamic-analysis" vs `build-dev`**: The issue spec mentions updating the upload-artifact in the "dynamic-analysis" job, but that job has no artifact upload. The spec was referring to the `build-dev` job (the dev artifact build).

## Output to keep in mind

- Both `claude_engine` and `claude_editor` are now in every release zip alongside `data/`.
- The `tests` job does not install libgtk-3-dev and does not need to — it only builds `core_tests`.
- If a future issue adds `-DBUILD_EDITOR=ON` to the `dynamic-analysis` cmake configure, libgtk-3-dev is already installed there.

## Skills used
- `projectSettings:impl-issue`
