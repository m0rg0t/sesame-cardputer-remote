"""Exercise the real packager with synthetic ESP32 images, never device firmware."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import struct
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('release_under_test', ROOT / 'tools/build_release.py')
release = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(release)


def image(*, version='0.1.0', project='SesameRemote', identity=True,
          magic=release.APP_DESC_MAGIC, second_segment=True):
    header = bytearray(24)
    header[0:2] = bytes((0xE9, 2 if second_segment else 1))
    struct.pack_into('<H', header, 12, 9)
    header[23] = 1
    payload = bytearray(256)
    struct.pack_into('<I', payload, 0, magic)
    payload[16:16 + len(version)] = version.encode()
    payload[48:48 + len(project)] = project.encode()
    if identity:
        payload += release.build_identity('0.1.0').encode() + b'\0'
    segments = [payload] + ([b'synthetic second segment'] if second_segment else [])
    result = header
    checksum = 0xEF
    for part in segments:
        result += struct.pack('<II', 0x3C000020, len(part)) + part
        for byte in part:
            checksum ^= byte
    result += bytes(15 - len(result) % 16) + bytes((checksum,))
    return bytes(result) + hashlib.sha256(result).digest()


class ImageValidationTests(unittest.TestCase):
    def test_accepts_valid_one_and_two_segment_images(self):
        for second in (False, True):
            with self.subTest(second=second):
                self.assertEqual(release.validate_image(image(second_segment=second),
                    release.build_identity('0.1.0')),
                    {'app_version': '0.1.0', 'project_name': 'SesameRemote'})

    def test_rejects_header_and_size_errors(self):
        for data in (b'', bytes(47), bytes(release.MAXIMUM + 1)):
            with self.subTest(length=len(data)):
                with self.assertRaisesRegex(ValueError, 'size bounds'):
                    release.validate_image(data)
        for offset, value in ((0, 0), (1, 0), (1, 17), (12, 0)):
            data = bytearray(image()); data[offset] = value
            with self.subTest(offset=offset, value=value):
                with self.assertRaisesRegex(ValueError, 'ESP32-S3 image'):
                    release.validate_image(data)

    def test_rejects_truncated_segment_header_and_payload(self):
        data = bytearray(image(second_segment=False))
        data[1] = 2
        size = struct.unpack_from('<I', data, 28)[0]
        with self.assertRaisesRegex(ValueError, 'Truncated segment header'):
            release.validate_image(data[:32 + size + 3])
        struct.pack_into('<I', data, 28, len(data))
        with self.assertRaisesRegex(ValueError, 'Truncated image segment'):
            release.validate_image(data)

    def test_rejects_checksum_hash_missing_hash_and_trailing_bytes(self):
        good = image()
        checksum = bytearray(good); checksum[-33] ^= 1
        digest = bytearray(good); digest[-1] ^= 1
        flag = bytearray(good); flag[23] = 0
        for bad, message in ((checksum, 'checksum'), (digest, 'SHA-256 mismatch'),
                             (flag, 'appended SHA-256'), (good[:-1], 'appended SHA-256'),
                             (good + b'extra', 'trailing content')):
            with self.subTest(message=message):
                with self.assertRaisesRegex(ValueError, message):
                    release.validate_image(bad)

    def test_rejects_bad_descriptor_and_identity(self):
        with self.assertRaisesRegex(ValueError, 'application descriptor'):
            release.validate_image(image(magic=0))
        with self.assertRaisesRegex(ValueError, 'identity marker'):
            release.validate_image(image(identity=False), release.build_identity('0.1.0'))
        data = bytearray(image()); data[32 + 16] = 255
        # Re-sign so this specifically exercises descriptor decoding.
        data[-33] ^= ord('0') ^ 255
        data[-32:] = hashlib.sha256(data[:-32]).digest()
        with self.assertRaisesRegex(ValueError, 'Invalid UTF-8'):
            release.validate_image(data)


class PackagingTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for name, text in {'VERSION': '0.1.0\n', 'platformio.ini': '[env:test]\n',
                           'src/main.cpp': '// synthetic\n', 'scripts/check.py': '# synthetic\n',
                           'tools/build_release.py': '# synthetic\n',
                           'docs/INSTALL.md': 'Synthetic fixture, do not install.\n'}.items():
            path = self.root / name; path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text)
        self.binary = self.root / '.pio/build/cardputer-adv-sesame/firmware.bin'
        self.binary.parent.mkdir(parents=True)
        self.binary.write_bytes(image())
        # Explicit timestamps avoid filesystem clock-granularity assumptions.
        for path in release.build_inputs(self.root):
            os.utime(path, ns=(1_000_000_000, 1_000_000_000))
        os.utime(self.binary, ns=(2_000_000_000, 2_000_000_000))

    def test_package_allowlist_hashes_and_verification_flags(self):
        archive, manifest = release.build_release(self.root)
        with zipfile.ZipFile(archive) as package:
            names = ['cardputer-sesame-robot-0.1.0.bin', 'INSTALL.md', 'manifest.json', 'SHA256SUMS']
            self.assertEqual(sorted(package.namelist()), sorted(names))
            self.assertEqual(json.loads(package.read('manifest.json')), manifest)
            for name, info in manifest['files'].items():
                data = package.read(name)
                self.assertEqual(len(data), info['bytes'])
                self.assertEqual(hashlib.sha256(data).hexdigest(), info['sha256'])
            for line in package.read('SHA256SUMS').decode().splitlines():
                digest, name = line.split('  ')
                self.assertEqual(hashlib.sha256(package.read(name)).hexdigest(), digest)
        latest = json.loads((self.root / 'dist/latest.json').read_text())
        self.assertEqual(latest['archive_sha256'], hashlib.sha256(archive.read_bytes()).hexdigest())
        self.assertFalse(manifest['hardware_verified'])
        self.assertFalse(manifest['robot_integration_verified'])
        self.assertTrue(manifest['app_only'])
        self.assertEqual(manifest['headroom_bytes'], release.MAXIMUM - len(image()))

    def test_rejects_noncanonical_binary_before_output(self):
        other = self.root / 'firmware.bin'; other.write_bytes(image())
        with self.assertRaisesRegex(ValueError, 'canonical'):
            release.build_release(self.root, other)
        self.assertFalse((self.root / 'dist').exists())

    def test_rejects_wrong_project_version_and_identity(self):
        for data, message in ((image(project='Other'), 'project name'),
                              (image(version='9.9.9'), 'application version'),
                              (image(identity=False), 'identity marker')):
            self.binary.write_bytes(data)
            with self.subTest(message=message):
                with self.assertRaisesRegex(ValueError, message):
                    release.build_release(self.root)
        self.assertFalse((self.root / 'dist').exists())

    def test_rejects_stale_binary_and_missing_input(self):
        os.utime(self.root / 'src/main.cpp', ns=(3_000_000_000, 3_000_000_000))
        with self.assertRaisesRegex(ValueError, 'rebuild first'):
            release.build_release(self.root)
        (self.root / 'platformio.ini').unlink()
        with self.assertRaisesRegex(ValueError, 'Missing build input'):
            release.source_hash(self.root)

    def test_rejects_unsafe_versions_and_unknown_release_entries(self):
        for version in ('', '../bad', '1.0/extra', '1.0\nother'):
            (self.root / 'VERSION').write_text(version)
            with self.subTest(version=version):
                with self.assertRaisesRegex(ValueError, 'Unsafe version'):
                    release.read_version(self.root)
        (self.root / 'VERSION').write_text('0.1.0')
        os.utime(self.root / 'VERSION', ns=(1_000_000_000, 1_000_000_000))
        directory = self.root / 'dist/cardputer-sesame-robot-0.1.0'
        directory.mkdir(parents=True); (directory / 'unexpected.txt').write_text('extra')
        with self.assertRaisesRegex(ValueError, 'Unexpected release entry'):
            release.build_release(self.root)

    def test_source_hash_changes_and_ignores_python_cache(self):
        before = release.source_hash(self.root)
        cache = self.root / 'tools/__pycache__/release.pyc'
        cache.parent.mkdir(); cache.write_bytes(b'cache')
        self.assertEqual(release.source_hash(self.root), before)
        (self.root / 'src/main.cpp').write_text('// changed\n')
        self.assertNotEqual(release.source_hash(self.root), before)


if __name__ == '__main__':
    unittest.main()
