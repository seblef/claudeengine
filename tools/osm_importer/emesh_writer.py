"""Write geometry to Wreckoning .emesh files."""

import struct

_MAGIC = b'EMSH'
_VERSION = 3
_MATERIAL_PATH_SIZE = 256

# Vertex = (position xyz, normal xyz, binormal xyz, tangent xyz, uv xy) — 14 floats / 56 bytes
Vertex = tuple[
    tuple[float, float, float],  # position
    tuple[float, float, float],  # normal
    tuple[float, float, float],  # binormal
    tuple[float, float, float],  # tangent
    tuple[float, float],         # uv
]


def write_emesh(path: str, vertices: list, indices: list, material_path: str) -> None:
    """Serialise vertices and indices as a single-submesh .emesh binary file at path.

    AABB is computed from vertex positions.  material_path is null-terminated
    and padded to 256 bytes (relative to data/, e.g. 'materials/road.yaml').
    """
    if not vertices:
        raise ValueError("vertices must not be empty")
    if not indices:
        raise ValueError("indices must not be empty")
    if len(indices) % 3 != 0:
        raise ValueError(f"index count {len(indices)} is not a multiple of 3")

    positions = [v[0] for v in vertices]
    aabb_min = (
        min(p[0] for p in positions),
        min(p[1] for p in positions),
        min(p[2] for p in positions),
    )
    aabb_max = (
        max(p[0] for p in positions),
        max(p[1] for p in positions),
        max(p[2] for p in positions),
    )

    mat_bytes = material_path.encode("utf-8")[:_MATERIAL_PATH_SIZE - 1]
    mat_bytes = mat_bytes + b'\x00' * (_MATERIAL_PATH_SIZE - len(mat_bytes))

    with open(path, 'wb') as f:
        # Header
        f.write(_MAGIC)
        f.write(struct.pack('<I', _VERSION))

        # Geometry header
        f.write(struct.pack('<II', len(vertices), len(indices)))
        f.write(struct.pack('<fff', *aabb_min))
        f.write(struct.pack('<fff', *aabb_max))

        # Vertices: px py pz  nx ny nz  bx by bz  tx ty tz  u v  (14 floats, 56 bytes)
        for pos, nor, binormal, tangent, uv in vertices:
            f.write(struct.pack('<14f', *pos, *nor, *binormal, *tangent, *uv))

        # Indices
        f.write(struct.pack(f'<{len(indices)}I', *indices))

        # SubMeshes — single submesh spanning all indices
        f.write(struct.pack('<I', 1))
        f.write(struct.pack('<II', 0, len(indices)))
        f.write(mat_bytes)
