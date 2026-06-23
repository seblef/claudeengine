# OSM Importer — Building Geometry → .emesh — issue #749

## What was done

Implemented `tools/osm_importer/buildings.py`: converts OSM closed-way building
footprints into extruded `.emesh` geometry files via `emesh_writer.write_emesh()`.

Added `tools/osm_importer/test_buildings.py`: 28 pytest cases covering all
filtering rules, height parsing, material selection, file creation, and
descriptor dict structure.

Updated `tools/osm_importer/main.py` to call `build_building_meshes()` and
print a summary line.

No `src/` or editor files were touched.

## Files changed

| File | Change |
|------|--------|
| `tools/osm_importer/buildings.py` | Full implementation (was a `NotImplementedError` stub) |
| `tools/osm_importer/test_buildings.py` | New — 28 pytest cases |
| `tools/osm_importer/main.py` | Import + call `build_building_meshes`; removed TODO(#749) |

## Algorithm implemented

### Filtering
- Skip ways missing a `building` tag.
- Skip open ways (first node_id ≠ last node_id) → `WARNING`.
- Project footprint nodes (excluding the closing duplicate); skip if < 3 remain → `WARNING`.
- Compute signed XZ area via the shoelace formula; skip if |area| < 1.0 m² → `WARNING`.

### Height resolution (in priority order)
1. `height` tag parsed as `float` (metres)
2. `building:levels` tag × 3.0 m
3. Default: 9.0 m

### Wall geometry (per footprint edge)
- Vertices 0–3: `(p0_bot, p1_bot, p1_top, p0_top)` — 4 verts / edge
- Triangles: `(0,1,2)` and `(0,2,3)` — CCW when viewed from outside
- Normal: `normalize(edge_dir × Y_up) = normalize(-dz, 0, dx)` — outward-facing
- Tangent: `normalize(p1 − p0)`, Binormal: `(0, 1, 0)`
- UVs: `u = cumulative_perimeter_distance / 3`, `v = vertex_y / 3`

### Roof geometry (triangle fan)
- Centroid vertex at `(cx, height, cz)` as fan apex (index 0 within roof vertex block)
- Perimeter vertices shared between adjacent fan triangles (n+1 vertices total, n triangles)
- Normal: `(0, 1, 0)`, Tangent: `(1, 0, 0)`, Binormal: `(0, 0, 1)`
- UVs: planar `(x / 10, z / 10)`

### Material mapping
| `building` tag value | material |
|---|---|
| `residential`, `apartments`, `house`, `detached` | `materials/building_residential.yaml` |
| `commercial`, `retail`, `office` | `materials/building_commercial.yaml` |
| anything else (incl. `yes`) | `materials/building_default.yaml` |

### Output
- `.emesh` written to `<mesh_dir>/<way_id>.emesh`; `mesh_dir` created if absent.
- Returns `list[dict]` with keys `name`, `type`, `mesh` for the map YAML assembler (#750).

## Decisions

* **`projector` added as 3rd positional parameter** — the existing stub lacked it;
  all geometry is projected from OSM lat/lon. The change is consistent with
  `extract_roads(ways, nodes, projector)`.
* **1 m² area threshold** — eliminates map-digitising artefacts (near-zero slivers)
  without discarding any real-world building.
* **Perimeter vertices shared in the roof fan** — saves n−1 vertices vs. duplicating
  per-triangle. UVs are planar so sharing is safe.
* **Wall vertex duplication per edge** — each of the 4 wall vertices carries a
  distinct UV, so no sharing is possible across edges; 4 verts/edge is the minimum.
* **CCW winding order from outside** — spec-mandated; validation against the engine
  render is deferred to issue #751.
* **Convex-fan roof** — deliberate POC simplification per the issue; courtyard
  buildings (concave polygons) will have incorrect roofs but acceptable walls.
* **No `src/` changes** — all geometry lives in the Python tooling layer.

## Output to keep in mind

* `build_building_meshes(ways, nodes, projector, mesh_dir)` signature.
* Descriptor dict keys: `name`, `type`, `mesh` (relative filename, no path prefix).
* Winding order must be verified against the engine in issue #751.
* Material files (`building_residential.yaml`, etc.) do not exist yet; they must be
  created before the engine can render these meshes.

## Skills / CLAUDE.md instructions used

* `impl-issue` skill — branch from `dev`, implement, commit, PR flow
* CLAUDE.md: `feat/` branch prefix, conventional commits, history file,
  zero modifications to `src/`
