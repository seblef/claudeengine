# fix(gldevices): GLTexture unbind after upload

**Date:** 2026-06-11  
**Issue:** #475  
**PR:** #482  
**Branch:** `fix/gldevices-gltexture-unbind-after-upload-475`

## Problem

`GLTexture::GLTexture()` called `glGenTextures` + `glBindTexture(GL_TEXTURE_2D, tex_id_)` to upload texture data but never called `glBindTexture(GL_TEXTURE_2D, 0)` before returning. Every other GL texture type (`GLTileableTexture`, `GLRawTexture`, `GLNormalMapTexture`, `GLRenderTarget`) already unbinds at the end of construction.

The consequence: every disk-loaded material texture permanently occupied whatever texture unit was last activated via `glActiveTexture` at construction time, silently leaving units in a dirty state. Any shader reading a unit without an explicit rebind first could see stale content — a bug that becomes more likely as scene texture count grows.

## Fix

Added a single line immediately after `glGenerateMipmap(GL_TEXTURE_2D)` in `src/gldevices/GLTexture.cpp`:

```cpp
glBindTexture(GL_TEXTURE_2D, 0);
```

This is identical to the cleanup pattern used by all other texture constructors in the same module.

## Decisions / Rationale

- No API changes, no header changes — pure internal cleanup.
- Placement after `glGenerateMipmap` mirrors the exact pattern in `GLTileableTexture` and `GLRawTexture`.
- Early-return paths (load failure) already exit before any `glBind*` call, so they are unaffected and do not need a matching unbind.

## Skills / instructions used

- `impl-issue` skill workflow (checkout dev → branch → implement → cpplint → commit → PR)
- `CLAUDE.md`: conventional commits, PR to `dev`, history file requirement
