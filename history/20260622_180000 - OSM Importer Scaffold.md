# OSM Importer Scaffold — issue #744

## What was done

Created `tools/osm_importer/` as the base package for the OSM → Wreckoning
map converter POC.  No engine or editor files were touched.

### Files created

| File | Purpose |
|------|---------|
| `__init__.py` | Package marker |
| `main.py` | argparse CLI entry point |
| `requirements.txt` | `requests` + `PyYAML` |
| `README.md` | Usage examples and install instructions |
| `fetcher.py` | Stub — Overpass API download |
| `projection.py` | Stub — lon/lat → local XZ projection |
| `emesh_writer.py` | Stub — .emesh serialiser |
| `roads.py` | Stub — road mesh builder |
| `buildings.py` | Stub — building mesh builder |
| `map_writer.py` | Stub — .map.yaml writer |

`.gitignore` was extended with `__pycache__/`, `*.pyc`, `*.pyo`.

## Decisions

* `--bbox` and `--osm-file` are **mutually exclusive** — both can't be
  meaningful at the same time and silently ignoring one would be surprising.
* The mutual-exclusion check is done in `main()` (not `argparse`) because
  `add_mutually_exclusive_group` would require both to be optional but then
  the error message would be less clear.
* Stub functions raise `NotImplementedError` rather than returning a sentinel
  so callers fail loudly if accidentally invoked before implementation.
* TODO comments in `main.py` reference the follow-up issue numbers (#745–#749)
  to maintain traceability.

## Output to keep in mind

* PR #754 → `dev`
* Each stub module has a single `TODO(#NNN)` comment pointing at the
  follow-up issue that will implement it.
* The CLI already validates argument combos, so follow-up issues can focus
  purely on pipeline logic.

## Skills / CLAUDE.md instructions used

* `impl-issue` skill — branch, implement, commit, PR flow
* CLAUDE.md: branch prefix `feat/`, conventional commits, history file
  requirement, zero modifications to `src/`
