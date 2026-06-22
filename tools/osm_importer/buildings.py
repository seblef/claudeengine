"""Build building mesh geometry from OSM way/relation data."""


def build_building_meshes(ways, nodes, mesh_dir: str) -> list:
    """Return a list of mesh descriptors written to *mesh_dir*."""
    # TODO(#748): extrude building footprints and call emesh_writer
    raise NotImplementedError
