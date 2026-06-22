"""Build road mesh geometry from OSM way data."""


def build_road_meshes(ways, nodes, mesh_dir: str) -> list:
    """Return a list of mesh descriptors written to *mesh_dir*."""
    # TODO(#747): tessellate road splines and call emesh_writer
    raise NotImplementedError
