# OSM Importer

Converts [OpenStreetMap](https://www.openstreetmap.org/) data into Wreckoning
`.map.yaml` + `.emesh` assets.

## Dependencies

Python 3.9+ is required.

```bash
pip install -r tools/osm_importer/requirements.txt
```

## Usage

### From a live bounding box (Overpass API)

```bash
python -m tools.osm_importer.main \
    --bbox "-0.12,51.50,-0.10,51.51" \
    --output data/maps/london_demo.map.yaml \
    --mesh-dir data/meshes/london_demo/
```

### From a pre-downloaded .osm file

```bash
# 1. Export from openstreetmap.org or use osmium / osmfilter
# 2. Run the importer offline
python -m tools.osm_importer.main \
    --osm-file my_area.osm \
    --output data/maps/my_area.map.yaml \
    --mesh-dir data/meshes/my_area/
```

## Module overview

| File | Responsibility |
|------|----------------|
| `main.py` | CLI entry point (argparse) |
| `fetcher.py` | Download OSM XML from Overpass API |
| `projection.py` | Project lon/lat to local XZ plane |
| `roads.py` | Build road mesh geometry |
| `buildings.py` | Build building mesh geometry |
| `emesh_writer.py` | Serialize geometry to `.emesh` files |
| `map_writer.py` | Write the final `.map.yaml` |
