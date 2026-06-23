# OSM Road Extraction → type:road YAML (#748)

## What was done

Implemented `tools/osm_importer/roads.py`: converts OSM `highway` ways into
`type: road` YAML descriptor dicts ready for emission into `.map.yaml`.

Wired the new function into `tools/osm_importer/main.py` alongside the
existing projection step.

Added `tools/osm_importer/test_roads.py` with 19 unit tests (all green).

## Key decisions

### Return dicts, not YAML strings
`extract_roads` returns a list of plain Python dicts. Serialisation to YAML
happens later (issue #749, `map_writer.py`). This keeps the road extractor
single-responsibility and easy to test without file I/O.

### Module-level lookup tables
`_WIDTHS` and `_MATERIALS` are module-level dicts with `dict.get(key, default)`
lookups. This is more readable than a chain of `if/elif` and easier to extend
without touching control flow.

### Projector passed in (dependency injection)
`extract_roads(ways, nodes, projector)` accepts any object with a
`project(lat, lon) -> (x, z)` method. Tests use a trivial identity projector —
no need to construct a real `Projector` with a bbox just to unit-test road
filtering logic.

### bbox fallback in main.py
When `--osm-file` is used (no CLI `--bbox`), `_bbox_from_args_or_nodes` derives
the bounding box from the loaded node coordinates. This lets the Projector be
constructed in all execution paths.

### Round control-point coordinates to 3 decimal places
`round(x, 3)` keeps YAML output readable (sub-millimetre precision is
irrelevant for road splines at city scale).

## Output to keep in mind

- `extract_roads` returns `list[dict]`; each dict has keys:
  `name`, `type`, `width`, `samples_per_metre`, `material`, `control_points`
- `control_points` is `list[[x: float, 0.0, z: float]]`
- `map_writer.py` (#749) must consume this list and serialise it as YAML
- The `buildings.py` TODO is a separate upcoming issue (no issue number
  assigned yet)

## Skills / CLAUDE.md guidance used

- `impl-issue` skill instructions (branch from dev, conventional commits, PR to dev)
- CLAUDE.md: no modifications to `src/` or editor/engine files (respected)
- CLAUDE.md: history file required for every contribution
