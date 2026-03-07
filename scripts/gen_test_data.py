#!/usr/bin/env python3
"""Generate synthetic test data for geoviz development.

Generates 100k points with random lon/lat in the English Channel area
and a random numeric 'speed' column.
"""

from __future__ import annotations

from pathlib import Path

import numpy as np
import pyarrow as pa
import pyarrow.parquet as pq


def generate_channel_data(
    n_points: int = 100_000,
    lon_range: tuple[float, float] = (-5.0, 2.0),
    lat_range: tuple[float, float] = (49.0, 51.5),
    speed_range: tuple[float, float] = (0.0, 25.0),
    seed: int = 42,
) -> pa.Table:
    """Generate random geospatial points in the English Channel."""
    rng = np.random.default_rng(seed)

    lon = rng.uniform(lon_range[0], lon_range[1], size=n_points).astype(np.float64)
    lat = rng.uniform(lat_range[0], lat_range[1], size=n_points).astype(np.float64)
    speed = rng.uniform(speed_range[0], speed_range[1], size=n_points).astype(np.float64)

    table = pa.table({
        "lon": lon,
        "lat": lat,
        "speed": speed,
    })
    return table


def generate_track_data(
    n_tracks: int = 50,
    points_per_track: int = 2000,
    seed: int = 123,
) -> pa.Table:
    """Generate synthetic vessel tracks (sorted by entity then time)."""
    rng = np.random.default_rng(seed)

    all_lon: list[np.ndarray] = []
    all_lat: list[np.ndarray] = []
    all_sog: list[np.ndarray] = []

    for _ in range(n_tracks):
        # Random start in English Channel
        start_lon = rng.uniform(-5.0, 1.0)
        start_lat = rng.uniform(49.0, 51.0)

        # Random walk
        lon_steps = rng.normal(0.001, 0.003, size=points_per_track)
        lat_steps = rng.normal(0.0005, 0.002, size=points_per_track)

        lons = np.cumsum(lon_steps) + start_lon
        lats = np.cumsum(lat_steps) + start_lat
        sog = np.abs(rng.normal(8.0, 4.0, size=points_per_track))

        all_lon.append(lons)
        all_lat.append(lats)
        all_sog.append(sog)

    table = pa.table({
        "lon": np.concatenate(all_lon),
        "lat": np.concatenate(all_lat),
        "sog": np.concatenate(all_sog),
    })
    return table


def main() -> None:
    out_dir = Path(__file__).resolve().parent.parent

    # Point data
    points = generate_channel_data()
    points_path = out_dir / "test_data.parquet"
    pq.write_table(points, points_path)
    print(f"Written {len(points)} rows to {points_path}")

    # Track data
    tracks = generate_track_data()
    tracks_path = out_dir / "vessel_tracks.parquet"
    pq.write_table(tracks, tracks_path)
    print(f"Written {len(tracks)} rows to {tracks_path}")


if __name__ == "__main__":
    main()
