#!/usr/bin/env python3
"""Download vendored dependencies: DuckDB amalgamation and cJSON."""

from __future__ import annotations

import io
import urllib.request
import zipfile
from pathlib import Path


DUCKDB_VERSION = "v1.2.1"
CJSON_VERSION = "v1.7.18"

PROJECT_ROOT = Path(__file__).resolve().parent.parent


def download_duckdb() -> None:
    """Download DuckDB amalgamation (duckdb.cpp + duckdb.h + duckdb.hpp)."""
    dest = PROJECT_ROOT / "third_party" / "duckdb"
    dest.mkdir(parents=True, exist_ok=True)

    if (dest / "duckdb.cpp").exists() and (dest / "duckdb.h").exists():
        print(f"DuckDB already present at {dest}")
        return

    url = (
        f"https://github.com/duckdb/duckdb/releases/download/{DUCKDB_VERSION}"
        f"/libduckdb-src.zip"
    )
    print(f"Downloading DuckDB {DUCKDB_VERSION} amalgamation...")
    resp = urllib.request.urlopen(url)
    data = resp.read()

    with zipfile.ZipFile(io.BytesIO(data)) as zf:
        for name in zf.namelist():
            if name.endswith((".cpp", ".h", ".hpp")):
                fname = Path(name).name
                target = dest / fname
                target.write_bytes(zf.read(name))
                print(f"  Extracted {fname}")

    print("DuckDB amalgamation downloaded.")


def download_cjson() -> None:
    """Download cJSON source files."""
    dest = PROJECT_ROOT / "third_party" / "cjson"
    dest.mkdir(parents=True, exist_ok=True)

    if (dest / "cJSON.c").exists() and (dest / "cJSON.h").exists():
        print(f"cJSON already present at {dest}")
        return

    base_url = (
        f"https://raw.githubusercontent.com/DaveGamble/cJSON/{CJSON_VERSION}"
    )

    for filename in ("cJSON.c", "cJSON.h"):
        url = f"{base_url}/{filename}"
        print(f"Downloading {filename}...")
        resp = urllib.request.urlopen(url)
        (dest / filename).write_bytes(resp.read())

    print("cJSON downloaded.")


def main() -> None:
    download_duckdb()
    download_cjson()
    print("\nAll dependencies downloaded. You can now build with CMake.")


if __name__ == "__main__":
    main()
