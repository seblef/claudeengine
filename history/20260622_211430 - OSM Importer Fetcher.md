# OSM Importer Fetcher — issue #745

## What was done

Implemented `tools/osm_importer/fetcher.py` with two data-acquisition paths:

1. **Overpass API** (`--bbox`): builds an Overpass QL query, POSTs to
   `https://overpass-api.de/api/interpreter`, parses the JSON response.
2. **Local file** (`--osm-file`): parses an `.osm` XML file using
   `xml.etree.ElementTree` (stdlib, zero extra deps).

Both paths return the same unified model consumed by all downstream modules:
```python
nodes: dict[int, tuple[float, float]]   # id → (lat, lon)
ways:  list[dict]                        # {id, tags, node_ids}
```

`main.py` was updated to call `load_osm()` and print the element counts,
replacing the `TODO(#745)` stub.

### Files changed

| File | Change |
|------|--------|
| `tools/osm_importer/fetcher.py` | Full implementation (replaced stub) |
| `tools/osm_importer/main.py` | Call `load_osm()`, drop no-op print |

## Decisions

* **`urllib.request` for HTTP** — `requests` is already in `requirements.txt`
  but using stdlib `urllib.request` keeps the Overpass path dependency-free at
  call time (no import risk if `requests` is not installed). The tradeoff is
  slightly more verbose code; acceptable since the call site is small.
* **Bbox reordering** — CLI takes `min_lon,min_lat,max_lon,max_lat` (GeoJSON
  convention); Overpass expects `(south,west,north,east)` = `(min_lat,min_lon,
  max_lat,max_lon)`. The reorder is done explicitly in `_build_overpass_query`
  with labelled variable names to make the swap obvious.
* **`_parse_overpass_json` is a pure function** — split out from `_fetch_from_overpass`
  so it can be unit-tested without network access.
* **No filtering of OSM nodes** — the fetcher returns *all* nodes in the
  response (Overpass already returns only nodes referenced by the requested
  ways via the `>;` step). Filtering by tag belongs in `roads.py` / `buildings.py`.

## Output to keep in mind

* PR → `dev` closes #745
* Downstream modules (`roads.py`, `buildings.py`) receive the unified
  `(nodes, ways)` tuple and must filter by `way["tags"].get("highway")` /
  `way["tags"].get("building")` themselves.
* The Overpass path uses `urllib.request.urlopen` which raises
  `urllib.error.HTTPError` / `urllib.error.URLError` on failure — callers
  can add a try/except later if needed.

## Skills / CLAUDE.md instructions used

* `impl-issue` skill — branch, implement, commit, PR flow
* CLAUDE.md: `feat/` branch prefix, conventional commits, history file,
  zero modifications to `src/`
