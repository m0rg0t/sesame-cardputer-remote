"""Finalize and size-check the application-only M5Apps image."""

import hashlib
from pathlib import Path
import struct

Import("env")

MAX_M5APPS_APP_BYTES = 0x140000
APP_DESC_MAGIC = 0xABCD5432
PROJECT_NAME = "SesameRemote"


def _field(value, size):
    encoded = value.encode("ascii")
    if len(encoded) >= size:
        raise RuntimeError(f"Application descriptor value is too long: {value}")
    return encoded + bytes(size - len(encoded))


def finalize_descriptor(firmware):
    data = bytearray(firmware.read_bytes())
    if len(data) < 48 or data[0] != 0xE9:
        raise RuntimeError("Expected an ESP application image")

    cursor = 24
    checksum = 0xEF
    first_payload = None
    for segment in range(data[1]):
        if cursor + 8 > len(data):
            raise RuntimeError("Truncated ESP image segment header")
        _, segment_size = struct.unpack_from("<II", data, cursor)
        cursor += 8
        if segment_size > len(data) - cursor:
            raise RuntimeError("Truncated ESP image segment")
        if segment == 0:
            first_payload = cursor
        cursor += segment_size

    if first_payload is None or struct.unpack_from("<I", data, first_payload)[0] != APP_DESC_MAGIC:
        raise RuntimeError("ESP application descriptor is missing")

    version = (Path(env.subst("$PROJECT_DIR")) / "VERSION").read_text().strip()
    data[first_payload + 16 : first_payload + 48] = _field(version, 32)
    data[first_payload + 48 : first_payload + 80] = _field(PROJECT_NAME, 32)

    # Descriptor edits affect the image checksum and appended validation hash.
    cursor = 24
    for _ in range(data[1]):
        _, segment_size = struct.unpack_from("<II", data, cursor)
        cursor += 8
        for byte in data[cursor : cursor + segment_size]:
            checksum ^= byte
        cursor += segment_size
    checksum_at = (cursor // 16 + 1) * 16 - 1
    image_end = checksum_at + 1
    if data[23] != 1 or len(data) != image_end + 32:
        raise RuntimeError("Expected one appended ESP image validation hash")
    data[checksum_at] = checksum
    data[image_end:] = hashlib.sha256(data[:image_end]).digest()
    firmware.write_bytes(data)


def check_size(source, target, env):
    firmware = Path(str(target[0]))
    finalize_descriptor(firmware)
    size = firmware.stat().st_size
    remaining = MAX_M5APPS_APP_BYTES - size
    print(
        f"Sesame Remote M5Apps image: {size:,} / "
        f"{MAX_M5APPS_APP_BYTES:,} bytes ({remaining:,} remaining)"
    )
    if remaining < 0:
        raise RuntimeError("Application exceeds the 0x140000-byte M5Apps profile")


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", check_size)
