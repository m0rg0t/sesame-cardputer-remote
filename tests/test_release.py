import importlib.util
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "build_release", ROOT / "tools" / "build_release.py"
)
BUILD_RELEASE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(BUILD_RELEASE)


class ReleaseTests(unittest.TestCase):
    def test_version_and_identity_are_safe(self):
        version = BUILD_RELEASE.read_version(ROOT)
        self.assertEqual(version, "0.1.0")
        self.assertEqual(
            BUILD_RELEASE.build_identity(version),
            "cardputer-sesame-robot/application/0.1.0",
        )

    def test_source_hash_is_stable_shape(self):
        digest = BUILD_RELEASE.source_hash(ROOT)
        self.assertEqual(len(digest), 64)
        int(digest, 16)


if __name__ == "__main__":
    unittest.main()
