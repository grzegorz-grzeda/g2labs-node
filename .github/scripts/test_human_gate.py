import unittest
from pathlib import Path

from human_gate import gated_files, load_patterns, verdict

PATTERNS = load_patterns((Path(__file__).parent.parent / "gated-paths").read_text())


class GatedPaths(unittest.TestCase):
    def assertGated(self, *files):
        self.assertEqual(gated_files(files, PATTERNS), list(files))

    def assertNotGated(self, *files):
        self.assertEqual(gated_files(files, PATTERNS), [])

    def test_gated(self):
        self.assertGated(
            ".github/workflows/ci.yml",
            ".github/workflows/sub/x.yml",
            ".github/gated-paths",
            ".github/scripts/human_gate.py",
            ".claude/skills/review-pr/SKILL.md",
            "sysbuild.conf",
            "sysbuild/mcuboot.conf",
            "keys/root-ec-p256.pem",
            "root.pem",
            "contract/topics.json",
            "contract/schemas/status.schema.json",
            "docs/adr/0006-merging-never-delivers.md",
            "VERSION",
        )

    def test_not_gated(self):
        self.assertNotGated(
            "core/src/core.c",
            "service/src/hub.ts",
            "docs/mqtt.md",
            "docs/issues/0034-human-gate-check-for-gated-paths.md",
            "docs/agents/development-flow.md",
            ".github/scripts/test_human_gate.py",
            "service/VERSION",
            "src/sysbuild.c",
            "prj.conf",
        )


class Verdict(unittest.TestCase):
    def test_ungated_passes(self):
        self.assertEqual(verdict(["core/src/core.c"], set(), PATTERNS), (True, "no gated paths"))

    def test_gated_fails_until_approved(self):
        passed, reason = verdict(["VERSION", "core/src/core.c"], set(), PATTERNS)
        self.assertFalse(passed)
        self.assertIn("VERSION", reason)
        self.assertTrue(verdict(["VERSION"], {"human-approved"}, PATTERNS)[0])

    def test_ready_for_human_fails_until_approved(self):
        self.assertFalse(verdict(["core/src/core.c"], {"ready-for-human"}, PATTERNS)[0])
        self.assertTrue(verdict(["core/src/core.c"], {"ready-for-human", "human-approved"}, PATTERNS)[0])

    def test_long_lists_are_shortened(self):
        files = [f"contract/examples/x/{i}.json" for i in range(5)]
        self.assertIn("and 2 more", verdict(files, set(), PATTERNS)[1])


if __name__ == "__main__":
    unittest.main()
