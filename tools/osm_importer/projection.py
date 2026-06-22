"""Project geographic coordinates (WGS84 lon/lat) to a flat local XZ plane."""

import math

METRES_PER_DEGREE = 111320.0


class Projector:
    """Equirectangular projection centred on the midpoint of a bounding box."""

    def __init__(self, bbox: tuple[float, float, float, float]):
        # bbox = (lat_min, lon_min, lat_max, lon_max)
        lat_min, lon_min, lat_max, lon_max = bbox
        self._lat_mid = (lat_min + lat_max) / 2.0
        self._lon_mid = (lon_min + lon_max) / 2.0
        self._cos_lat = math.cos(math.radians(self._lat_mid))

        lat_span_m = (lat_max - lat_min) * METRES_PER_DEGREE
        lon_span_m = (lon_max - lon_min) * self._cos_lat * METRES_PER_DEGREE
        self._map_size = math.ceil(max(lat_span_m, lon_span_m) / 10.0) * 10.0

    def project(self, lat: float, lon: float) -> tuple[float, float]:
        """Return (x, z) in metres, origin at bbox centre."""
        x = (lon - self._lon_mid) * self._cos_lat * METRES_PER_DEGREE
        z = (lat - self._lat_mid) * METRES_PER_DEGREE
        return x, z

    def map_size(self) -> float:
        """Return the longest axis of the bbox in metres, rounded up to the nearest 10."""
        return self._map_size
