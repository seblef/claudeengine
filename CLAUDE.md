# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Protected Files

**DO NOT DELETE the following files under any circumstances:**
- `/CLAUDE.md` (this file)
- Any project-level `CLAUDE.md` files

## Repository Overview

Claude Engine is a repository for a **multi-platform 3D shoot'em up game engine**. It is written in modern C++ and support both Windows and Linux.

## Repository Structure

The repository contains four folders:

* **src** contains all of the engine C++ source code
* **data** contains data for game levels (maps, meshes, actors, textures, materials, shaders, etc.), resources for the editor (icons, etc.) and configuration files
* **tests** contains source code tests
* **external** contains source code or library for third-party libraries
* **history** contains the MarkDown files you output from contributions, this is your memory store

Each folder includes a CLAUDE.md file providing details about commands and architecture. **Always read the folder-specific CLAUDE.md first** when working in a subdirectory.

## Game Engine main features

* 3D rendering based on deferred lighting principle
* In-game world and resources edition (material, meshes geometry, entity positions, lights, etc.)

## Tech stack

* All source code is written in modern C++
* All configuration files and most of resources files should be stored in yaml format: materials, etc.
* OpenGL 4.6 and DirectX 12 must be supported for rendering
* IMGUI is used for editing in-game resources
* loguru is used for logging

## Building

The whole source code should be built in a single executable, in the `build` folder.

CMake is used to build the executable. There should be two options corresponding to two levels of maturity:
* `dev`: less compiler optimization, more logging, and the executable must embed debugging symbols
* `stable`: more agressive compiler optimization, less logging and the executable must be kept small

Whenever possible, try to keep compilation time low.

## Guidelines (CRITICAL)

All of written code must be **optimized but readable for a good C++ developer**

Any contribution must be accompanied by:
* a MarkDown file, named `{DATETIME} - {FEATURE}.md`, stored in `history` folder, and containing a comprehensive description of the changes, the decisions made and their rationale, and the output to keep in mind for you for the next features.
* an proposal to update the README.md if building or installing process have been updated (e.g., using a new library)
* propositions to improve the `CLAUDE.md` files, if it can avoid unecessary questions from you
* propositions to improve my specifictions, if it can avoir unecessary questions from you
* the list of skills files you used
* the instructions from skills files and `CLAUDE.md` files you especially took into account to contribute