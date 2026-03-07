"""Tests for the test data generation scripts."""

from __future__ import annotations

import numpy as np
import pyarrow as pa
import pytest

# Import the generation functions directly
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "scripts"))
from gen_test_data import generate_channel_data, generate_track_data


class TestChannelData:
    def test_row_count(self) -> None:
        table = generate_channel_data(n_points=1000)
        assert len(table) == 1000

    def test_columns_exist(self) -> None:
        table = generate_channel_data(n_points=100)
        assert "lon" in table.column_names
        assert "lat" in table.column_names
        assert "speed" in table.column_names

    def test_lon_range(self) -> None:
        table = generate_channel_data(n_points=10_000)
        lon = table.column("lon").to_numpy()
        assert lon.min() >= -5.0
        assert lon.max() <= 2.0

    def test_lat_range(self) -> None:
        table = generate_channel_data(n_points=10_000)
        lat = table.column("lat").to_numpy()
        assert lat.min() >= 49.0
        assert lat.max() <= 51.5

    def test_speed_range(self) -> None:
        table = generate_channel_data(n_points=10_000)
        speed = table.column("speed").to_numpy()
        assert speed.min() >= 0.0
        assert speed.max() <= 25.0

    def test_deterministic_with_seed(self) -> None:
        t1 = generate_channel_data(n_points=100, seed=42)
        t2 = generate_channel_data(n_points=100, seed=42)
        np.testing.assert_array_equal(
            t1.column("lon").to_numpy(), t2.column("lon").to_numpy()
        )

    def test_different_seeds_differ(self) -> None:
        t1 = generate_channel_data(n_points=100, seed=1)
        t2 = generate_channel_data(n_points=100, seed=2)
        assert not np.array_equal(
            t1.column("lon").to_numpy(), t2.column("lon").to_numpy()
        )

    def test_custom_ranges(self) -> None:
        table = generate_channel_data(
            n_points=500,
            lon_range=(10.0, 20.0),
            lat_range=(30.0, 40.0),
            speed_range=(5.0, 10.0),
        )
        lon = table.column("lon").to_numpy()
        lat = table.column("lat").to_numpy()
        speed = table.column("speed").to_numpy()
        assert lon.min() >= 10.0 and lon.max() <= 20.0
        assert lat.min() >= 30.0 and lat.max() <= 40.0
        assert speed.min() >= 5.0 and speed.max() <= 10.0

    def test_dtypes(self) -> None:
        table = generate_channel_data(n_points=10)
        assert table.schema.field("lon").type == pa.float64()
        assert table.schema.field("lat").type == pa.float64()
        assert table.schema.field("speed").type == pa.float64()


class TestTrackData:
    def test_row_count(self) -> None:
        table = generate_track_data(n_tracks=5, points_per_track=100)
        assert len(table) == 500

    def test_columns_exist(self) -> None:
        table = generate_track_data(n_tracks=2, points_per_track=50)
        assert "lon" in table.column_names
        assert "lat" in table.column_names
        assert "sog" in table.column_names

    def test_sog_non_negative(self) -> None:
        table = generate_track_data(n_tracks=10, points_per_track=200)
        sog = table.column("sog").to_numpy()
        assert (sog >= 0).all()

    def test_deterministic(self) -> None:
        t1 = generate_track_data(n_tracks=3, points_per_track=50, seed=99)
        t2 = generate_track_data(n_tracks=3, points_per_track=50, seed=99)
        np.testing.assert_array_equal(
            t1.column("lon").to_numpy(), t2.column("lon").to_numpy()
        )


class TestSpecFiles:
    """Validate that test spec JSON files are well-formed."""

    @pytest.fixture
    def spec_dir(self) -> Path:
        return Path(__file__).resolve().parent.parent / "test_specs"

    def test_points_spec_valid_json(self, spec_dir: Path) -> None:
        import json

        data = json.loads((spec_dir / "points.json").read_text())
        assert "data" in data
        assert "layers" in data
        assert data["layers"][0]["mark"] == "point"

    def test_tracks_spec_valid_json(self, spec_dir: Path) -> None:
        import json

        data = json.loads((spec_dir / "tracks.json").read_text())
        assert data["basemap"] == "satellite"
        assert data["layers"][0]["mark"] == "line"

    def test_nautical_spec_valid_json(self, spec_dir: Path) -> None:
        import json

        data = json.loads((spec_dir / "nautical.json").read_text())
        assert data["basemap"] == "nautical"
        assert len(data["layers"]) == 2
