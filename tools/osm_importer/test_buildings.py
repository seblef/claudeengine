"""Unit tests for buildings.build_building_meshes."""

import os
import tempfile

import pytest

from osm_importer.buildings import (
    _parse_height,
    _polygon_area,
    build_building_meshes,
)


class _IdentityProjector:
    """Projects (lat, lon) → (lon, lat) so test coords are easy to reason about."""
    def project(self, lat, lon):
        return lon, lat


_PROJ = _IdentityProjector()


def _square_nodes():
    """Four corners of a 10 × 10 square (in lon/lat, identity-projected to x/z)."""
    return {
        1: (0.0, 0.0),
        2: (0.0, 10.0),
        3: (10.0, 10.0),
        4: (10.0, 0.0),
    }


def _square_way(building="yes", way_id=200, extra_tags=None):
    tags = {"building": building}
    if extra_tags:
        tags.update(extra_tags)
    return {"id": way_id, "tags": tags, "node_ids": [1, 2, 3, 4, 1]}


# ---------------------------------------------------------------------------
# _parse_height
# ---------------------------------------------------------------------------

def test_parse_height_from_height_tag():
    assert _parse_height({"height": "12.5"}) == pytest.approx(12.5)


def test_parse_height_from_levels_tag():
    assert _parse_height({"building:levels": "3"}) == pytest.approx(9.0)


def test_parse_height_default():
    assert _parse_height({}) == pytest.approx(9.0)


def test_parse_height_prefers_height_over_levels():
    assert _parse_height({"height": "8.0", "building:levels": "5"}) == pytest.approx(8.0)


def test_parse_height_invalid_height_falls_back_to_levels():
    assert _parse_height({"height": "n/a", "building:levels": "2"}) == pytest.approx(6.0)


def test_parse_height_invalid_both_returns_default():
    assert _parse_height({"height": "n/a", "building:levels": "bad"}) == pytest.approx(9.0)


# ---------------------------------------------------------------------------
# _polygon_area
# ---------------------------------------------------------------------------

def test_polygon_area_square():
    pts = [(0.0, 0.0), (10.0, 0.0), (10.0, 10.0), (0.0, 10.0)]
    assert abs(_polygon_area(pts)) == pytest.approx(100.0)


def test_polygon_area_triangle():
    pts = [(0.0, 0.0), (4.0, 0.0), (0.0, 3.0)]
    assert abs(_polygon_area(pts)) == pytest.approx(6.0)


# ---------------------------------------------------------------------------
# build_building_meshes — filtering
# ---------------------------------------------------------------------------

def test_skip_way_without_building_tag():
    way = {"id": 1, "tags": {"highway": "residential"}, "node_ids": [1, 2, 3, 1]}
    with tempfile.TemporaryDirectory() as d:
        result = build_building_meshes([way], _square_nodes(), _PROJ, d)
    assert result == []


def test_skip_open_way(caplog):
    way = {"id": 2, "tags": {"building": "yes"}, "node_ids": [1, 2, 3, 4]}
    with tempfile.TemporaryDirectory() as d:
        result = build_building_meshes([way], _square_nodes(), _PROJ, d)
    assert result == []
    assert "not a closed way" in caplog.text


def test_skip_fewer_than_3_nodes(caplog):
    nodes = {1: (0.0, 0.0), 2: (0.0, 5.0)}
    way = {"id": 3, "tags": {"building": "yes"}, "node_ids": [1, 2, 1]}
    with tempfile.TemporaryDirectory() as d:
        result = build_building_meshes([way], nodes, _PROJ, d)
    assert result == []
    assert "fewer than 3" in caplog.text


def test_skip_near_zero_area(caplog):
    # Three collinear points
    nodes = {1: (0.0, 0.0), 2: (0.0, 5.0), 3: (0.0, 10.0)}
    way = {"id": 4, "tags": {"building": "yes"}, "node_ids": [1, 2, 3, 1]}
    with tempfile.TemporaryDirectory() as d:
        result = build_building_meshes([way], nodes, _PROJ, d)
    assert result == []
    assert "near-zero" in caplog.text


def test_skip_missing_nodes(caplog):
    nodes = {1: (0.0, 0.0), 2: (0.0, 10.0)}  # nodes 3,4 absent → only 2 pts
    with tempfile.TemporaryDirectory() as d:
        result = build_building_meshes([_square_way()], nodes, _PROJ, d)
    assert result == []


# ---------------------------------------------------------------------------
# build_building_meshes — happy path
# ---------------------------------------------------------------------------

def test_writes_emesh_file():
    with tempfile.TemporaryDirectory() as d:
        build_building_meshes([_square_way(way_id=42)], _square_nodes(), _PROJ, d)
        assert os.path.isfile(os.path.join(d, "42.emesh"))


def test_emesh_file_non_empty():
    with tempfile.TemporaryDirectory() as d:
        build_building_meshes([_square_way(way_id=42)], _square_nodes(), _PROJ, d)
        assert os.path.getsize(os.path.join(d, "42.emesh")) > 0


def test_returns_descriptor_dict():
    with tempfile.TemporaryDirectory() as d:
        result = build_building_meshes([_square_way(way_id=99)], _square_nodes(), _PROJ, d)
    assert len(result) == 1
    desc = result[0]
    assert desc["name"] == "building_99"
    assert desc["type"] == "mesh"
    assert desc["mesh"] == "99.emesh"


def test_multiple_buildings():
    ways = [_square_way(way_id=10), _square_way(way_id=11)]
    with tempfile.TemporaryDirectory() as d:
        result = build_building_meshes(ways, _square_nodes(), _PROJ, d)
        assert len(result) == 2
        assert os.path.isfile(os.path.join(d, "10.emesh"))
        assert os.path.isfile(os.path.join(d, "11.emesh"))


def test_creates_mesh_dir_if_missing():
    with tempfile.TemporaryDirectory() as parent:
        mesh_dir = os.path.join(parent, "meshes", "sub")
        build_building_meshes([_square_way()], _square_nodes(), _PROJ, mesh_dir)
        assert os.path.isdir(mesh_dir)


# ---------------------------------------------------------------------------
# build_building_meshes — material selection
# ---------------------------------------------------------------------------

@pytest.mark.parametrize("tag", ["residential", "apartments", "house", "detached"])
def test_material_residential(tag):
    with tempfile.TemporaryDirectory() as d:
        result = build_building_meshes([_square_way(building=tag)], _square_nodes(), _PROJ, d)
    assert result[0]["mesh"] == f"{_square_way(building=tag)['id']}.emesh"


@pytest.mark.parametrize("tag", ["commercial", "retail", "office"])
def test_material_commercial(tag):
    with tempfile.TemporaryDirectory() as d:
        result = build_building_meshes([_square_way(building=tag)], _square_nodes(), _PROJ, d)
    assert len(result) == 1


def test_material_default_for_yes_tag():
    with tempfile.TemporaryDirectory() as d:
        result = build_building_meshes([_square_way(building="yes")], _square_nodes(), _PROJ, d)
    assert len(result) == 1


# ---------------------------------------------------------------------------
# build_building_meshes — height variants
# ---------------------------------------------------------------------------

def test_height_tag_accepted():
    way = _square_way(extra_tags={"height": "15.0"})
    with tempfile.TemporaryDirectory() as d:
        result = build_building_meshes([way], _square_nodes(), _PROJ, d)
    assert len(result) == 1


def test_levels_tag_accepted():
    way = _square_way(extra_tags={"building:levels": "4"})
    with tempfile.TemporaryDirectory() as d:
        result = build_building_meshes([way], _square_nodes(), _PROJ, d)
    assert len(result) == 1
