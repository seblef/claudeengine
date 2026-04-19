# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Protected Files

**DO NOT DELETE the following files under any circumstances:**
- `/CLAUDE.md` (this file)
- Any project-level `CLAUDE.md` files

## Repository overview

This folder contains all of the game engine source code. The codebase is written in modern C++.

## Repository structure

The codebase is splitted in folders:

* **app** contains game engine entrypoint. It must not implement any logic other than loading configuration and running the engine.
* **core** contains common modules, functions and classes: linear algebra (vectors, matrices, quaternions, planes, etc.), abstract and concrete filesystems, yaml file parsing, geometry utilities (bouding boxes, bounding spheres, etc.), color utilities, etc.
* **abstract** contains all abstract classes for rendering (video device, vertex and index buffers, textures, shaders, etc.), audio (sound, sound sources, listener, etc.) and input (window event, keyboard, mouse)

Each folder includes a CLAUDE.md file providing details about commands and architecture. **Always read the folder-specific CLAUDE.md first** when working in a subdirectory.

## Guidelines (CRITICAL)

* A `.cpp` file must not implement more than one class.
* A `.h` file must not declare more than one class
* Utility functions (e.g., fast inverse square root) must be implemented in a single `*Utils.cpp` file
* Google C++ style guide must be followed
* Document meaningfully the exposed classes, functions, constants, etc.