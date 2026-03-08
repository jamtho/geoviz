#!/usr/bin/env python3
"""Generate a small parquet fixture for C data loading tests."""

from __future__ import annotations

from pathlib import Path

import numpy as np
import pyarrow as pa
import pyarrow.parquet as pq


def main() -> None:
    table = pa.table({
        "lon": np.array([-3.0, -2.0, -1.0, 0.0], dtype=np.float64),
        "lat": np.array([50.0, 50.5, 51.0, 50.2], dtype=np.float64),
        "speed": np.array([8.0, 12.0, 6.0, 18.0], dtype=np.float64),
    })

    out = Path(__file__).resolve().parent / "fixture.parquet"
    pq.write_table(table, out)
    print(f"Written {len(table)} rows to {out}")


if __name__ == "__main__":
    main()
