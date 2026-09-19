#!/usr/bin/env python3
"""Create and verify an allowlisted, application-only M5Apps package."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import zipfile

ROOT = Path(__file__).resolve().parents[1]
MAXIMUM = 0x140000
APP_DESC_MAGIC = 0xABCD5432
IDENTITY_PREFIX = "cardputer-sesame-robot/application/"
EXPECTED_PROJECT_NAME = "SesameRemote"
SAFE_VALUE = re.compile(r"^[0-9a-z.-]+$")


def read_version(root=ROOT):
    version = (root / "VERSION").read_text().strip()
    if not version or not SAFE_VALUE.fullmatch(version):
        raise ValueError("Unsafe version")
    return version


def build_identity(version):
    return IDENTITY_PREFIX + version


def sha(data):
    return hashlib.sha256(data).hexdigest()


def _descriptor_text(data, offset, size, label):
    raw = data[offset : offset + size]
    value = raw.split(b"\0", 1)[0]
    try:
        return value.decode("utf-8")
    except UnicodeDecodeError as error:
        raise ValueError(f"Invalid UTF-8 in application {label}") from error


def validate_image(data, expected_identity=None):
    if len(data) < 48 or len(data) > MAXIMUM:
        raise ValueError("Image outside compact application size bounds")
    if (
        data[0] != 0xE9
        or not 1 <= data[1] <= 16
        or struct.unpack_from("<H", data, 12)[0] != 9
    ):
        raise ValueError("Expected an ESP32-S3 image")

    cursor = 24
    checksum = 0xEF
    first_payload = None
    first_size = 0
    for segment in range(data[1]):
        if cursor + 8 > len(data):
            raise ValueError("Truncated segment header")
        _, size = struct.unpack_from("<II", data, cursor)
        cursor += 8
        if size > len(data) - cursor:
            raise ValueError("Truncated image segment")
        if segment == 0:
            first_payload = cursor
            first_size = size
        for byte in data[cursor : cursor + size]:
            checksum ^= byte
        cursor += size

    checksum_at = (cursor // 16 + 1) * 16 - 1
    if checksum_at >= len(data) or data[checksum_at] != checksum:
        raise ValueError("Image checksum mismatch")
    image_end = checksum_at + 1
    if data[23] != 1 or len(data) != image_end + 32:
        raise ValueError("Expected an appended SHA-256 and no trailing content")
    if hashlib.sha256(data[:image_end]).digest() != data[image_end:]:
        raise ValueError("Image SHA-256 mismatch")

    if (
        first_payload is None
        or first_size < 256
        or struct.unpack_from("<I", data, first_payload)[0] != APP_DESC_MAGIC
    ):
        raise ValueError("Expected an ESP32-S3 application descriptor")
    metadata = {
        "app_version": _descriptor_text(data, first_payload + 16, 32, "version"),
        "project_name": _descriptor_text(
            data, first_payload + 48, 32, "project name"
        ),
    }
    if expected_identity is not None:
        marker = expected_identity.encode("ascii") + b"\0"
        if marker not in data:
            raise ValueError("Project identity marker missing from application image")
    return metadata


def build_inputs(root=ROOT):
    paths = [root / "VERSION", root / "platformio.ini"]
    for folder in ("src", "scripts", "tools"):
        paths.extend(
            path
            for path in sorted((root / folder).rglob("*"))
            if path.is_file()
            and "__pycache__" not in path.parts
            and path.suffix != ".pyc"
        )
    missing = [path for path in paths if not path.is_file()]
    if missing:
        raise ValueError("Missing build input: " + str(missing[0]))
    return paths


def source_hash(root=ROOT):
    digest = hashlib.sha256()
    for path in build_inputs(root):
        digest.update(path.relative_to(root).as_posix().encode())
        digest.update(b"\0")
        digest.update(path.read_bytes())
    return digest.hexdigest()


def ensure_fresh(root, binary):
    inputs = [
        path
        for path in build_inputs(root)
        if path.relative_to(root).parts[0] != "tools"
    ]
    newest = max(path.stat().st_mtime_ns for path in inputs)
    if newest > binary.stat().st_mtime_ns:
        raise ValueError("Build inputs changed after the BIN; rebuild first")


def build_release(root=ROOT, bin_path=None):
    root = Path(root).resolve()
    canonical = (
        root / ".pio/build/cardputer-adv-sesame/firmware.bin"
    ).resolve()
    selected = canonical if bin_path is None else Path(bin_path).resolve()
    if selected != canonical:
        raise ValueError("Only the canonical Cardputer ADV artifact may be packaged")

    version = read_version(root)
    data = selected.read_bytes()
    metadata = validate_image(data, build_identity(version))
    if metadata["project_name"] != EXPECTED_PROJECT_NAME:
        raise ValueError(
            "Unexpected application project name: " + metadata["project_name"]
        )
    if metadata["app_version"] != version:
        raise ValueError(
            "Unexpected application version: " + metadata["app_version"]
        )
    ensure_fresh(root, selected)

    package_name = "cardputer-sesame-robot-" + version
    directory = root / "dist" / package_name
    directory.mkdir(parents=True, exist_ok=True)
    allowed = {
        package_name + ".bin",
        "INSTALL.md",
        "manifest.json",
        "SHA256SUMS",
    }
    unknown = sorted(
        path.name for path in directory.iterdir() if path.name not in allowed
    )
    if unknown:
        raise ValueError("Unexpected release entry: " + unknown[0])

    files = {
        package_name + ".bin": data,
        "INSTALL.md": (root / "docs/INSTALL.md").read_bytes(),
    }
    manifest = {
        "version": version,
        "status": "release_candidate",
        "target": "ESP32-S3 / M5Stack Cardputer ADV",
        "app_only": True,
        "hardware_verified": False,
        "robot_integration_verified": False,
        "image_project_name": metadata["project_name"],
        "image_app_version": metadata["app_version"],
        "build_identity": build_identity(version),
        "max_app_bytes": MAXIMUM,
        "app_bytes": len(data),
        "headroom_bytes": MAXIMUM - len(data),
        "source_sha256": source_hash(root),
        "default_wifi": "Sesame-Controller",
        "mdns_host": "sesame-robot.local",
        "wifi_credentials_persisted": False,
        "files": {
            name: {"bytes": len(content), "sha256": sha(content)}
            for name, content in files.items()
        },
        "note": (
            "Application size alone does not prove compatibility with a "
            "particular installed M5Apps partition table."
        ),
    }
    files["manifest.json"] = (json.dumps(manifest, indent=2) + "\n").encode()
    files["SHA256SUMS"] = "".join(
        f"{sha(content)}  {name}\n" for name, content in files.items()
    ).encode()

    for name, content in files.items():
        (directory / name).write_bytes(content)

    archive = root / "dist" / (package_name + ".zip")
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as package:
        for name, content in files.items():
            package.writestr(name, content)
    with zipfile.ZipFile(archive) as package:
        if sorted(package.namelist()) != sorted(files):
            raise ValueError("Release archive member allowlist mismatch")
        for name, content in files.items():
            if package.read(name) != content:
                raise ValueError("Release archive content mismatch: " + name)

    latest = {
        **manifest,
        "directory": package_name,
        "archive": archive.name,
        "archive_sha256": sha(archive.read_bytes()),
    }
    (root / "dist/latest.json").write_text(json.dumps(latest, indent=2) + "\n")
    return archive, manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--bin",
        type=Path,
        default=ROOT / ".pio/build/cardputer-adv-sesame/firmware.bin",
    )
    args = parser.parse_args()
    try:
        archive, manifest = build_release(ROOT, args.bin)
    except (OSError, ValueError) as error:
        raise SystemExit(str(error)) from error
    binary_name = archive.stem + ".bin"
    print(
        f"{archive}: {manifest['app_bytes']:,}/{MAXIMUM:,} bytes; "
        f"SHA-256 {manifest['files'][binary_name]['sha256']}"
    )


if __name__ == "__main__":
    main()
