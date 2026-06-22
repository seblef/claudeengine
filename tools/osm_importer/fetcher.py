"""Fetch or parse OSM data and return a unified in-memory model."""

import urllib.request
import json
import xml.etree.ElementTree as ET

_OVERPASS_URL = "https://overpass-api.de/api/interpreter"

# Output model:
#   nodes: dict[int, tuple[float, float]]  — id → (lat, lon)
#   ways:  list[dict]                      — {id, tags, node_ids}


def load_osm(*, bbox: str = None, osm_file: str = None):
    """Return (nodes, ways) from either the Overpass API or a local .osm file."""
    if osm_file is not None:
        return _parse_osm_xml(osm_file)
    return _fetch_from_overpass(bbox)


# ---------------------------------------------------------------------------
# Overpass API path
# ---------------------------------------------------------------------------

def _build_overpass_query(bbox: str) -> str:
    # CLI bbox: min_lon,min_lat,max_lon,max_lat
    # Overpass bbox: (south,west,north,east) = (min_lat,min_lon,max_lat,max_lon)
    parts = bbox.split(",")
    min_lon, min_lat, max_lon, max_lat = parts
    overpass_bbox = f"{min_lat},{min_lon},{max_lat},{max_lon}"
    return (
        f"[out:json];"
        f"(way[highway]({overpass_bbox});"
        f"way[building]({overpass_bbox}););"
        f">;out body;"
    )


def _fetch_from_overpass(bbox: str):
    query = _build_overpass_query(bbox)
    body = query.encode("utf-8")
    req = urllib.request.Request(
        _OVERPASS_URL,
        data=body,
        headers={"Content-Type": "application/x-www-form-urlencoded"},
        method="POST",
    )
    with urllib.request.urlopen(req) as resp:
        data = json.load(resp)
    return _parse_overpass_json(data)


def _parse_overpass_json(data: dict):
    nodes = {}
    ways = []
    for elem in data.get("elements", []):
        if elem["type"] == "node":
            nodes[elem["id"]] = (elem["lat"], elem["lon"])
        elif elem["type"] == "way":
            ways.append({
                "id": elem["id"],
                "tags": elem.get("tags", {}),
                "node_ids": elem.get("nodes", []),
            })
    return nodes, ways


# ---------------------------------------------------------------------------
# Local .osm XML path
# ---------------------------------------------------------------------------

def _parse_osm_xml(path: str):
    tree = ET.parse(path)
    root = tree.getroot()

    nodes = {}
    for node in root.iter("node"):
        nid = int(node.attrib["id"])
        nodes[nid] = (float(node.attrib["lat"]), float(node.attrib["lon"]))

    ways = []
    for way in root.iter("way"):
        node_ids = [int(nd.attrib["ref"]) for nd in way.iter("nd")]
        tags = {tag.attrib["k"]: tag.attrib["v"] for tag in way.iter("tag")}
        ways.append({
            "id": int(way.attrib["id"]),
            "tags": tags,
            "node_ids": node_ids,
        })

    return nodes, ways
