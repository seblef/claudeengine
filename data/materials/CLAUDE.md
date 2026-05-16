# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this folder.

## Folder overview

This folder contains all of the game engine materials. Materials are loaded at the game layer
via `game::GameMaterial`, which wraps `renderer::Material`.

## Material format

* Materials are stored as YAML files named `{name}.yaml`.
* All fields are optional; unset fields fall back to engine defaults.
* The top-level key is `rendering:`. Additional sections (`sound:`, `physics:`) will be added in future milestones.

A complete material file looks like:

```yaml
rendering:
  textures:
    diffuse: demo.png
    normal: normal.png
    specular: specular.png
    emissive: emissive.png
    environment: env.png
  diffuse_color: [1.0, 1.0, 1.0, 1.0]
  emissive_color: [0.0, 0.0, 0.0, 0.0]
  ambient_color: [0.0, 0.0, 0.0, 0.0]
  shininess: 32
  cast_shadow: true
```

## Loading materials

Use `game::GameMaterial::GetOrLoad(name, video)` where `name` is the filename without
the `.yaml` extension (relative to `data/materials/`). Release with `mat->Release()` when done.

Example: to load `data/materials/demo.yaml`, call `GameMaterial::GetOrLoad("demo", video)`.
