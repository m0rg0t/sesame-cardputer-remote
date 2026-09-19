import hashlib
import json
from pathlib import Path
import struct
import unittest


ROOT = Path(__file__).resolve().parents[1]
SCREEN_DIR = ROOT / "docs" / "media" / "screens"


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def png_size(path: Path) -> tuple[int, int]:
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n" or data[12:16] != b"IHDR":
        raise ValueError(f"invalid PNG: {path}")
    return struct.unpack(">II", data[16:24])


class ScreenshotTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.manifest = json.loads(
            (SCREEN_DIR / "manifest.json").read_text(encoding="utf-8")
        )

    def test_manifest_matches_firmware(self):
        self.assertEqual(
            self.manifest["version"],
            (ROOT / "VERSION").read_text(encoding="utf-8").strip(),
        )
        self.assertEqual(
            self.manifest["source_sha256"], sha256(ROOT / "src" / "main.cpp")
        )
        self.assertEqual(self.manifest["native_size"], [240, 135])
        self.assertEqual(self.manifest["scale"], 3)

    def test_every_capture_exists_with_expected_size_and_hash(self):
        self.assertGreaterEqual(len(self.manifest["screens"]), 5)
        for screen in self.manifest["screens"]:
            with self.subTest(screen=screen["id"]):
                native = SCREEN_DIR / screen["native"]
                large = SCREEN_DIR / screen["large"]
                self.assertEqual(png_size(native), (240, 135))
                self.assertEqual(png_size(large), (720, 405))
                self.assertEqual(sha256(native), screen["native_sha256"])
                self.assertEqual(sha256(large), screen["large_sha256"])

    def test_site_uses_renderer_captures(self):
        html = (ROOT / "docs" / "index.html").read_text(encoding="utf-8")
        for screen in self.manifest["screens"]:
            self.assertIn(f"media/screens/{screen['large']}", html)
        self.assertNotIn("LIVE CONTROL DEMO", html)

    def test_readme_links_original_robot(self):
        readme = (ROOT / "README.md").read_text(encoding="utf-8")
        self.assertIn("https://github.com/dorianborian/sesame-robot", readme)


if __name__ == "__main__":
    unittest.main()
