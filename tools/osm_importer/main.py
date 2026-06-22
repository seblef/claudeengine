"""Entry point for the OSM → Wreckoning map converter."""

import argparse
import sys

from osm_importer.fetcher import load_osm
from osm_importer.projection import Projector
from osm_importer.roads import extract_roads


def parse_args(argv=None):
    parser = argparse.ArgumentParser(
        description="Convert OpenStreetMap data into a Wreckoning .map.yaml file."
    )
    parser.add_argument(
        "--bbox",
        metavar="MIN_LON,MIN_LAT,MAX_LON,MAX_LAT",
        help="Bounding box to fetch from the Overpass API (ignored when --osm-file is set).",
    )
    parser.add_argument(
        "--osm-file",
        metavar="PATH",
        help="Path to a pre-downloaded .osm XML file (skips network fetch).",
    )
    parser.add_argument(
        "--output",
        required=True,
        metavar="PATH",
        help="Destination .map.yaml file to write.",
    )
    parser.add_argument(
        "--mesh-dir",
        required=True,
        metavar="DIR",
        help="Directory where generated .emesh files are written.",
    )
    return parser.parse_args(argv)


def _bbox_from_args_or_nodes(bbox_str, nodes):
    """Return (lat_min, lon_min, lat_max, lon_max) from CLI string or node coords."""
    if bbox_str is not None:
        min_lon, min_lat, max_lon, max_lat = (float(v) for v in bbox_str.split(","))
        return min_lat, min_lon, max_lat, max_lon
    lats = [lat for lat, _ in nodes.values()]
    lons = [lon for _, lon in nodes.values()]
    return min(lats), min(lons), max(lats), max(lons)


def main(argv=None):
    args = parse_args(argv)

    if args.bbox is None and args.osm_file is None:
        print("error: one of --bbox or --osm-file is required", file=sys.stderr)
        sys.exit(1)

    if args.bbox is not None and args.osm_file is not None:
        print("error: --bbox and --osm-file are mutually exclusive", file=sys.stderr)
        sys.exit(1)

    nodes, ways = load_osm(bbox=args.bbox, osm_file=args.osm_file)
    print(f"Loaded {len(nodes)} nodes, {len(ways)} ways")

    bbox_tuple = _bbox_from_args_or_nodes(args.bbox, nodes)
    projector = Projector(bbox_tuple)

    roads = extract_roads(ways, nodes, projector)
    print(f"Extracted {len(roads)} roads")

    # TODO: generate building meshes
    # TODO(#749): write .map.yaml


if __name__ == "__main__":
    main()
