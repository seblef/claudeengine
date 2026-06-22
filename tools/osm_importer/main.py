"""Entry point for the OSM → Wreckoning map converter."""

import argparse
import sys


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


def main(argv=None):
    args = parse_args(argv)

    if args.bbox is None and args.osm_file is None:
        print("error: one of --bbox or --osm-file is required", file=sys.stderr)
        sys.exit(1)

    if args.bbox is not None and args.osm_file is not None:
        print("error: --bbox and --osm-file are mutually exclusive", file=sys.stderr)
        sys.exit(1)

    # TODO(#745): fetch OSM data via Overpass API when --bbox is given
    # TODO(#746): project lon/lat to local XZ plane
    # TODO(#747): generate road meshes
    # TODO(#748): generate building meshes
    # TODO(#749): write .map.yaml

    print("OSM importer scaffold — no-op stub")


if __name__ == "__main__":
    main()
