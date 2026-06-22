"""Project geographic coordinates (lon/lat) to a flat local XZ plane."""


def project(lon: float, lat: float, origin_lon: float, origin_lat: float):
    """Return (x, z) in metres relative to *origin_lon*/*origin_lat*."""
    # TODO(#746): implement equirectangular or Mercator projection
    raise NotImplementedError
