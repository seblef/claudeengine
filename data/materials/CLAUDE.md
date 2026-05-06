# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this folder.

## Folder overview

This folder contains all of the game engine materials.

## Material format

* Materials are stored as YAML files.
* All fields in materials are optional, the system has default values for them, any field can safely be ignored if not relevant

A complete material file looks like:

```yaml
textures:
  diffuse: demo.png
  normal: normal.png
  specular: specular.png
  emissive: emissive.png
  environment: env.png
```
