"""Build building mesh geometry from OSM way/node data."""

import logging
import math
import os

from osm_importer.emesh_writer import write_emesh

_LOGGER = logging.getLogger(__name__)

_DEFAULT_HEIGHT = 9.0
_LEVEL_HEIGHT = 3.0
_MIN_AREA = 1.0  # m²

_MATERIALS = {
    "residential": "materials/building_residential.yaml",
    "apartments":  "materials/building_residential.yaml",
    "house":       "materials/building_residential.yaml",
    "detached":    "materials/building_residential.yaml",
    "commercial":  "materials/building_commercial.yaml",
    "retail":      "materials/building_commercial.yaml",
    "office":      "materials/building_commercial.yaml",
}
_DEFAULT_MATERIAL = "materials/building_default.yaml"


def _parse_height(tags: dict) -> float:
    if "height" in tags:
        try:
            return float(tags["height"])
        except ValueError:
            pass
    if "building:levels" in tags:
        try:
            return int(tags["building:levels"]) * _LEVEL_HEIGHT
        except ValueError:
            pass
    return _DEFAULT_HEIGHT


def _polygon_area(points: list) -> float:
    """Signed XZ area via the shoelace formula."""
    n = len(points)
    total = 0.0
    for i in range(n):
        x0, z0 = points[i]
        x1, z1 = points[(i + 1) % n]
        total += x0 * z1 - x1 * z0
    return total * 0.5


def _normalize(v: tuple) -> tuple:
    x, y, z = v
    length = math.sqrt(x * x + y * y + z * z)
    if length < 1e-9:
        return (0.0, 0.0, 0.0)
    return (x / length, y / length, z / length)


def _build_walls(points: list, height: float) -> tuple:
    """Return (vertices, indices) for all walls of a footprint polygon."""
    vertices = []
    indices = []
    cumulative = 0.0
    n = len(points)
    for i in range(n):
        x0, z0 = points[i]
        x1, z1 = points[(i + 1) % n]
        dx = x1 - x0
        dz = z1 - z0
        edge_len = math.sqrt(dx * dx + dz * dz)
        if edge_len < 1e-9:
            continue
        tangent = _normalize((dx, 0.0, dz))
        normal = _normalize((-dz, 0.0, dx))  # edge_dir × up
        binormal = (0.0, 1.0, 0.0)
        base = len(vertices)
        u0 = cumulative / 3.0
        u1 = (cumulative + edge_len) / 3.0
        # 4 vertices: p0_bot, p1_bot, p1_top, p0_top
        vertices.append(((x0, 0.0, z0), normal, binormal, tangent, (u0, 0.0)))
        vertices.append(((x1, 0.0, z1), normal, binormal, tangent, (u1, 0.0)))
        vertices.append(((x1, height, z1), normal, binormal, tangent, (u1, height / 3.0)))
        vertices.append(((x0, height, z0), normal, binormal, tangent, (u0, height / 3.0)))
        indices.extend([base, base + 1, base + 2, base, base + 2, base + 3])
        cumulative += edge_len
    return vertices, indices


def _build_roof(points: list, height: float) -> tuple:
    """Return (vertices, indices) for a triangle-fan roof at *height*."""
    cx = sum(p[0] for p in points) / len(points)
    cz = sum(p[1] for p in points) / len(points)
    normal = (0.0, 1.0, 0.0)
    tangent = (1.0, 0.0, 0.0)
    binormal = (0.0, 0.0, 1.0)
    # Index 0 = centroid; indices 1..n = perimeter vertices (shared between fans)
    vertices = [((cx, height, cz), normal, binormal, tangent, (cx / 10.0, cz / 10.0))]
    for x, z in points:
        vertices.append(((x, height, z), normal, binormal, tangent, (x / 10.0, z / 10.0)))
    n = len(points)
    indices = []
    for i in range(n):
        indices.extend([0, 1 + i, 1 + (i + 1) % n])
    return vertices, indices


def build_building_meshes(ways: list, nodes: dict, projector, mesh_dir: str) -> list:
    """Write .emesh for each OSM building way; return list of mesh descriptors."""
    os.makedirs(mesh_dir, exist_ok=True)
    descriptors = []
    for way in ways:
        tags = way["tags"]
        if "building" not in tags:
            continue
        node_ids = way["node_ids"]
        if not node_ids or node_ids[0] != node_ids[-1]:
            _LOGGER.warning("Building %s skipped: not a closed way", way["id"])
            continue
        pts = []
        for nid in node_ids[:-1]:
            if nid not in nodes:
                continue
            lat, lon = nodes[nid]
            x, z = projector.project(lat, lon)
            pts.append((x, z))
        if len(pts) < 3:
            _LOGGER.warning("Building %s skipped: fewer than 3 projected nodes", way["id"])
            continue
        if abs(_polygon_area(pts)) < _MIN_AREA:
            _LOGGER.warning("Building %s skipped: near-zero footprint area", way["id"])
            continue
        height = _parse_height(tags)
        material = _MATERIALS.get(tags["building"], _DEFAULT_MATERIAL)
        wall_verts, wall_idxs = _build_walls(pts, height)
        roof_verts, roof_idxs = _build_roof(pts, height)
        wall_count = len(wall_verts)
        roof_idxs_offset = [idx + wall_count for idx in roof_idxs]
        vertices = wall_verts + roof_verts
        indices = wall_idxs + roof_idxs_offset
        out_path = os.path.join(mesh_dir, f"{way['id']}.emesh")
        write_emesh(out_path, vertices, indices, material)
        descriptors.append({
            "name": f"building_{way['id']}",
            "type": "mesh",
            "mesh": f"{way['id']}.emesh",
        })
    return descriptors
