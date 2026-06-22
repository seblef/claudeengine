"""Unit tests for emesh_writer — verifies the binary .emesh layout byte-by-byte."""

import os
import struct
import tempfile

import pytest

from osm_importer.emesh_writer import write_emesh

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

_DUMMY = (0.0, 0.0, 0.0)
_DUMMY_UV = (0.0, 0.0)


def _cube_vertices():
    """Eight unit-cube corners (0..1) with zeroed normals/binormal/tangent/UV."""
    positions = [
        (0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (1.0, 1.0, 0.0), (0.0, 1.0, 0.0),
        (0.0, 0.0, 1.0), (1.0, 0.0, 1.0), (1.0, 1.0, 1.0), (0.0, 1.0, 1.0),
    ]
    return [(p, _DUMMY, _DUMMY, _DUMMY, _DUMMY_UV) for p in positions]


def _cube_indices():
    return [
        0, 1, 2,  0, 2, 3,   # -Z face
        4, 6, 5,  4, 7, 6,   # +Z face
        0, 4, 5,  0, 5, 1,   # -Y face
        2, 6, 7,  2, 7, 3,   # +Y face
        0, 3, 7,  0, 7, 4,   # -X face
        1, 5, 6,  1, 6, 2,   # +X face
    ]


def _write_cube(material="materials/test.yaml"):
    path = tempfile.mktemp(suffix='.emesh')
    write_emesh(path, _cube_vertices(), _cube_indices(), material)
    return path


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

def test_creates_non_empty_file():
    path = _write_cube()
    try:
        assert os.path.getsize(path) > 0
    finally:
        os.unlink(path)


def test_header_magic_and_version():
    path = _write_cube()
    try:
        with open(path, 'rb') as f:
            magic = f.read(4)
            version, = struct.unpack('<I', f.read(4))
        assert magic == b'EMSH'
        assert version == 3
    finally:
        os.unlink(path)


def test_geometry_counts():
    vertices = _cube_vertices()
    indices = _cube_indices()
    path = _write_cube()
    try:
        with open(path, 'rb') as f:
            f.seek(8)
            vertex_count, index_count = struct.unpack('<II', f.read(8))
        assert vertex_count == len(vertices)
        assert index_count == len(indices)
    finally:
        os.unlink(path)


def test_aabb():
    path = _write_cube()
    try:
        with open(path, 'rb') as f:
            f.seek(16)  # magic(4) + version(4) + vertex_count(4) + index_count(4)
            aabb_min = struct.unpack('<fff', f.read(12))
            aabb_max = struct.unpack('<fff', f.read(12))
        assert aabb_min == (0.0, 0.0, 0.0)
        assert aabb_max == (1.0, 1.0, 1.0)
    finally:
        os.unlink(path)


def test_vertex_data_size():
    """Vertex block must be exactly vertex_count × 56 bytes."""
    vertices = _cube_vertices()
    indices = _cube_indices()
    path = _write_cube()
    try:
        vertex_block_start = 8 + 8 + 24   # header + counts + aabb
        expected = len(vertices) * 56
        with open(path, 'rb') as f:
            f.seek(vertex_block_start)
            data = f.read(expected)
        assert len(data) == expected
        # First vertex position must round-trip correctly
        px, py, pz = struct.unpack('<fff', data[:12])
        assert (px, py, pz) == (0.0, 0.0, 0.0)
    finally:
        os.unlink(path)


def test_submesh_block():
    vertices = _cube_vertices()
    indices = _cube_indices()
    material = "materials/road.yaml"
    path = _write_cube(material)
    try:
        # header(8) + counts(8) + aabb(24) + vertices(8×56) + indices(36×4)
        offset = 8 + 8 + 24 + len(vertices) * 56 + len(indices) * 4
        with open(path, 'rb') as f:
            f.seek(offset)
            submesh_count, = struct.unpack('<I', f.read(4))
            index_offset, index_count = struct.unpack('<II', f.read(8))
            mat_raw = f.read(256)
        assert submesh_count == 1
        assert index_offset == 0
        assert index_count == len(indices)
        assert mat_raw[:len(material)] == material.encode('utf-8')
        assert mat_raw[len(material)] == 0  # null terminator present
    finally:
        os.unlink(path)


def test_material_path_padded_to_256():
    path = _write_cube("mat/short.yaml")
    try:
        vertices = _cube_vertices()
        indices = _cube_indices()
        offset = 8 + 8 + 24 + len(vertices) * 56 + len(indices) * 4 + 4 + 8
        with open(path, 'rb') as f:
            f.seek(offset)
            mat_raw = f.read(256)
        assert len(mat_raw) == 256
        assert all(b == 0 for b in mat_raw[len("mat/short.yaml"):])
    finally:
        os.unlink(path)


def test_raises_on_empty_vertices():
    with pytest.raises(ValueError, match="vertices"):
        write_emesh("/tmp/unused.emesh", [], [0, 1, 2], "mat.yaml")


def test_raises_on_empty_indices():
    with pytest.raises(ValueError, match="indices"):
        write_emesh("/tmp/unused.emesh", _cube_vertices(), [], "mat.yaml")


def test_raises_on_non_multiple_of_3_indices():
    with pytest.raises(ValueError, match="multiple of 3"):
        write_emesh("/tmp/unused.emesh", _cube_vertices(), [0, 1], "mat.yaml")
