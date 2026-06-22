"""Download OSM data from the Overpass API for a given bounding box."""


def fetch_osm(bbox: str) -> str:
    """Return raw OSM XML for *bbox* (``min_lon,min_lat,max_lon,max_lat``)."""
    # TODO(#745): implement Overpass query and HTTP download
    raise NotImplementedError
