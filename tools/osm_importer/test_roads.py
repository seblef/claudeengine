"""Unit tests for roads.extract_roads."""

import pytest

from osm_importer.roads import extract_roads


# ---------------------------------------------------------------------------
# Minimal stub projector — returns (lon, lat) as (x, z) so values are easy
# to assert without floating-point surprises.
# ---------------------------------------------------------------------------

class _IdentityProjector:
    def project(self, lat, lon):
        return lon, lat


_PROJ = _IdentityProjector()

_NODES = {
    1: (0.0, 0.0),
    2: (1.0, 2.0),
    3: (3.0, 4.0),
}


def _way(highway, node_ids=(1, 2, 3), way_id=100):
    return {"id": way_id, "tags": {"highway": highway}, "node_ids": list(node_ids)}


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

def test_primary_road_descriptor():
    roads = extract_roads([_way("primary")], _NODES, _PROJ)
    assert len(roads) == 1
    r = roads[0]
    assert r["name"] == "road_100"
    assert r["type"] == "road"
    assert r["width"] == 10.0
    assert r["samples_per_metre"] == 1.0
    assert r["material"] == "materials/road_primary.yaml"
    assert r["control_points"] == [[0.0, 0.0, 0.0], [2.0, 0.0, 1.0], [4.0, 0.0, 3.0]]


def test_motorway_width_and_material():
    roads = extract_roads([_way("motorway")], _NODES, _PROJ)
    assert roads[0]["width"] == 14.0
    assert roads[0]["material"] == "materials/road_motorway.yaml"


def test_trunk_same_as_motorway():
    roads = extract_roads([_way("trunk")], _NODES, _PROJ)
    assert roads[0]["width"] == 14.0
    assert roads[0]["material"] == "materials/road_motorway.yaml"


def test_secondary_road():
    roads = extract_roads([_way("secondary")], _NODES, _PROJ)
    assert roads[0]["width"] == 8.0
    assert roads[0]["material"] == "materials/road_secondary.yaml"


def test_tertiary_uses_defaults():
    roads = extract_roads([_way("tertiary")], _NODES, _PROJ)
    assert roads[0]["width"] == 6.0
    assert roads[0]["material"] == "materials/road_residential.yaml"


def test_residential_road():
    roads = extract_roads([_way("residential")], _NODES, _PROJ)
    assert roads[0]["width"] == 6.0
    assert roads[0]["material"] == "materials/road_residential.yaml"


def test_living_street_road():
    roads = extract_roads([_way("living_street")], _NODES, _PROJ)
    assert roads[0]["width"] == 6.0


def test_unknown_highway_uses_defaults():
    roads = extract_roads([_way("track")], _NODES, _PROJ)
    assert roads[0]["width"] == 6.0
    assert roads[0]["material"] == "materials/road_residential.yaml"


@pytest.mark.parametrize("highway", [
    "footway", "path", "steps", "pedestrian", "cycleway", "bridleway",
])
def test_skip_non_drivable(highway):
    roads = extract_roads([_way(highway)], _NODES, _PROJ)
    assert roads == []


def test_skip_way_without_highway_tag():
    way = {"id": 99, "tags": {"building": "yes"}, "node_ids": [1, 2, 3]}
    assert extract_roads([way], _NODES, _PROJ) == []


def test_skip_way_with_fewer_than_two_nodes():
    single_node = {1: (0.0, 0.0)}
    roads = extract_roads([_way("residential", node_ids=[1])], single_node, _PROJ)
    assert roads == []


def test_skip_way_with_missing_nodes():
    roads = extract_roads([_way("residential", node_ids=[999, 998])], _NODES, _PROJ)
    assert roads == []


def test_missing_nodes_are_skipped_individually():
    nodes = {1: (0.0, 0.0), 3: (3.0, 4.0)}  # node 2 absent
    roads = extract_roads([_way("residential", node_ids=[1, 2, 3])], nodes, _PROJ)
    assert len(roads) == 1
    assert len(roads[0]["control_points"]) == 2


def test_multiple_ways():
    ways = [_way("primary", way_id=1), _way("secondary", way_id=2)]
    roads = extract_roads(ways, _NODES, _PROJ)
    assert len(roads) == 2
    assert roads[0]["name"] == "road_1"
    assert roads[1]["name"] == "road_2"
