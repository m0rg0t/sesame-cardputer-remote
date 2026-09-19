#!/usr/bin/env python3
"""Build the native firmware renderer and export checked-in PNG screenshots."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import struct
import subprocess
import zlib


ROOT = Path(__file__).resolve().parents[1]
RAW = ROOT / "build" / "screens"
OUT = ROOT / "docs" / "media" / "screens"
SCENARIOS = (
    ("home-online", "Pose picker with the robot online"),
    ("move-forward", "Forward movement held; release sends stop"),
    ("launch-wave", "Wave choreography launch animation"),
    ("wifi-list", "Nearby 2.4 GHz network picker"),
    ("mdns-discovery", "mDNS robot discovery"),
)


def read_ppm(path: Path) -> tuple[int, int, bytes]:
    with path.open("rb") as source:
        if source.readline().strip() != b"P6":
            raise ValueError(f"not a binary PPM: {path}")
        dimensions = source.readline().split()
        if len(dimensions) != 2:
            raise ValueError(f"invalid PPM dimensions: {path}")
        width, height = (int(value) for value in dimensions)
        if source.readline().strip() != b"255":
            raise ValueError(f"unsupported PPM depth: {path}")
        pixels = source.read()
    if len(pixels) != width * height * 3:
        raise ValueError(f"truncated PPM: {path}")
    return width, height, pixels


def scale_nearest(width: int, height: int, pixels: bytes, factor: int) -> bytes:
    rows: list[bytes] = []
    stride = width * 3
    for row_index in range(height):
        row = pixels[row_index * stride : (row_index + 1) * stride]
        scaled = b"".join(row[index : index + 3] * factor for index in range(0, stride, 3))
        rows.extend([scaled] * factor)
    return b"".join(rows)


def png_chunk(kind: bytes, payload: bytes) -> bytes:
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF)


def write_png(path: Path, width: int, height: int, pixels: bytes) -> None:
    stride = width * 3
    scanlines = b"".join(b"\0" + pixels[offset : offset + stride] for offset in range(0, len(pixels), stride))
    data = b"\x89PNG\r\n\x1a\n"
    data += png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    data += png_chunk(b"IDAT", zlib.compress(scanlines, 9))
    data += png_chunk(b"IEND", b"")
    path.write_bytes(data)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    subprocess.run(["pio", "run", "-e", "native-sim"], cwd=ROOT, check=True)
    RAW.mkdir(parents=True, exist_ok=True)
    subprocess.run([str(ROOT / ".pio" / "build" / "native-sim" / "program"), "--shots", str(RAW)], cwd=ROOT, check=True)
    OUT.mkdir(parents=True, exist_ok=True)

    entries = []
    for identifier, label in SCENARIOS:
        width, height, pixels = read_ppm(RAW / f"{identifier}.ppm")
        if (width, height) != (240, 135):
            raise ValueError(f"unexpected display size for {identifier}: {width}x{height}")
        native = OUT / f"{identifier}.png"
        large = OUT / f"{identifier}@3x.png"
        write_png(native, width, height, pixels)
        write_png(large, width * 3, height * 3, scale_nearest(width, height, pixels, 3))
        entries.append(
            {
                "id": identifier,
                "label": label,
                "native": native.name,
                "native_sha256": sha256(native),
                "large": large.name,
                "large_sha256": sha256(large),
            }
        )

    manifest = {
        "version": (ROOT / "VERSION").read_text(encoding="utf-8").strip(),
        "source": "src/main.cpp compiled by the M5GFX SDL native harness",
        "source_sha256": sha256(ROOT / "src" / "main.cpp"),
        "native_size": [240, 135],
        "scale": 3,
        "screens": entries,
    }
    (OUT / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"exported {len(entries)} firmware-renderer screenshots to {OUT}")


if __name__ == "__main__":
    main()
