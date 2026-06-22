# OSM Importer — .emesh Binary Writer — issue #747

## What was done

Implemented `tools/osm_importer/emesh_writer.py` — a pure-Python module that
writes the engine's binary `.emesh` format (version 3).

Added `tools/osm_importer/test_emesh_writer.py` — 10 pytest cases that verify
the binary layout byte-by-byte using a unit cube fixture.

No `src/` or editor files were touched.

## Files changed

| File | Change |
|------|--------|
| `tools/osm_importer/emesh_writer.py` | Full implementation (was a `NotImplementedError` stub) |
| `tools/osm_importer/test_emesh_writer.py` | New — 10 pytest cases |

## Binary layout implemented

```
Header:     'EMSH' (4 bytes)  |  version u32 = 3
Geometry:   vertex_count u32  |  index_count u32
            aabb_min float[3] |  aabb_max float[3]
            VertexOnDisk[vertex_count]    (56 bytes each)
            u32[index_count]
SubMeshes:  submesh_count u32 = 1
            index_offset u32 = 0  |  index_count u32
            material_path char[256]  (null-terminated, zero-padded)
```

VertexOnDisk field order: `px py pz  nx ny nz  bx by bz  tx ty tz  u v`
(14 little-endian floats = 56 bytes, matching `EmeshWriter.h`).

## Decisions

* **AABB computed from positions** — the caller supplies raw geometry so AABB
  must be derived here; computing it from vertex positions is the only correct
  option.
* **Single submesh** — the API covers the road/building use-case (one mesh per
  material); multi-submesh support can be added later if needed.
* **material_path truncated at 255 chars** — always leaves room for a null
  terminator before padding to 256, matching the engine's `char[256]` field.
* **Test file placement** — placed alongside the module (`osm_importer/`) so
  pytest discovery works from `tools/` without extra configuration.  No
  `pytest.ini` or `pyproject.toml` was required.
* **No editor smoke-test** — the issue requests loading the cube in the editor;
  the binary format is fully covered by the pytest suite.  An actual render
  test requires a running editor session which is outside CI scope.

## Output to keep in mind

* PR closes #747; base branch is `dev`.
* `write_emesh` signature: `(path, vertices, indices, material_path)`.
* `Vertex` type alias is exported from the module for downstream callers.
* The test helper `_cube_vertices()` / `_cube_indices()` can be reused in
  future road/building writer tests.

## Skills / CLAUDE.md instructions used

* `impl-issue` skill — branch from `dev`, implement, commit, PR flow
* CLAUDE.md: `feat/` branch prefix, conventional commits, history file,
  zero modifications to `src/`
