import hashlib
import json
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
MEDIA_DIR = ROOT / "docs" / "media" / "video"


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


class VideoTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.manifest = json.loads(
            (MEDIA_DIR / "manifest.json").read_text(encoding="utf-8")
        )

    def test_web_assets_match_manifest(self):
        for key in ("web", "poster"):
            with self.subTest(asset=key):
                entry = self.manifest[key]
                path = ROOT / entry["path"]
                self.assertEqual(path.stat().st_size, entry["bytes"])
                self.assertEqual(sha256(path), entry["sha256"])

    def test_mp4_is_small_and_fast_start(self):
        path = ROOT / self.manifest["web"]["path"]
        data = path.read_bytes()
        self.assertLess(len(data), 10 * 1024 * 1024)
        self.assertEqual(data[4:8], b"ftyp")
        self.assertIn(b"avc1", data[:200_000])
        self.assertIn(b"mp4a", data[:200_000])
        self.assertGreater(data.find(b"moov"), 0)
        self.assertGreater(data.find(b"mdat"), data.find(b"moov"))

    def test_poster_is_jpeg(self):
        path = ROOT / self.manifest["poster"]["path"]
        data = path.read_bytes()
        self.assertTrue(data.startswith(b"\xff\xd8\xff"))
        self.assertTrue(data.endswith(b"\xff\xd9"))

    def test_site_and_readme_reference_video(self):
        html = (ROOT / "docs" / "index.html").read_text(encoding="utf-8")
        readme = (ROOT / "README.md").read_text(encoding="utf-8")
        for name in (
            "cardputer-control-sesame.mp4",
            "cardputer-control-sesame-poster.jpg",
        ):
            self.assertIn(name, html)
            self.assertIn(name, readme)
        self.assertIn("controls", html)
        self.assertIn("playsinline", html)


if __name__ == "__main__":
    unittest.main()
