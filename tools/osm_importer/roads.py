"""Extract road descriptors from OSM way data and emit type:road YAML entries."""

_SKIP_TYPES = frozenset(
    {"footway", "path", "steps", "pedestrian", "cycleway", "bridleway"}
)

_WIDTHS = {
    "motorway": 14.0,
    "trunk": 14.0,
    "primary": 10.0,
    "secondary": 8.0,
    "tertiary": 6.0,
    "residential": 6.0,
    "living_street": 6.0,
}
_DEFAULT_WIDTH = 6.0

_MATERIALS = {
    "motorway": "materials/road_motorway.yaml",
    "trunk": "materials/road_motorway.yaml",
    "primary": "materials/road_primary.yaml",
    "secondary": "materials/road_secondary.yaml",
}
_DEFAULT_MATERIAL = "materials/road_residential.yaml"


def extract_roads(ways: list, nodes: dict, projector) -> list:
    """Return a list of road descriptor dicts for all drivable highway ways.

    Each dict is ready to be serialised as a ``type: road`` YAML entry.
    Ways with fewer than two resolvable nodes are silently skipped.
    """
    roads = []
    for way in ways:
        highway = way["tags"].get("highway")
        if not highway or highway in _SKIP_TYPES:
            continue

        control_points = []
        for nid in way["node_ids"]:
            if nid not in nodes:
                continue
            lat, lon = nodes[nid]
            x, z = projector.project(lat, lon)
            control_points.append([round(x, 3), 0.0, round(z, 3)])

        if len(control_points) < 2:
            continue

        roads.append({
            "name": f"road_{way['id']}",
            "type": "road",
            "width": _WIDTHS.get(highway, _DEFAULT_WIDTH),
            "samples_per_metre": 1.0,
            "material": _MATERIALS.get(highway, _DEFAULT_MATERIAL),
            "control_points": control_points,
        })
    return roads
