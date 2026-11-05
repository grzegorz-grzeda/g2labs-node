import unittest
from pathlib import Path

from check_version import check, read_version


def version_file(major=0, minor=3, patch=0, tweak=0, extra="dev"):
    return (
        f"VERSION_MAJOR = {major}\nVERSION_MINOR = {minor}\nPATCHLEVEL = {patch}\n"
        f"VERSION_TWEAK = {tweak}\nEXTRAVERSION = {extra}\n"
    )


class ReadVersion(unittest.TestCase):
    def test_dev_and_release(self):
        self.assertEqual(read_version(version_file(extra="dev")), "0.3.0-dev")
        self.assertEqual(read_version(version_file(extra="")), "0.3.0")

    def test_the_tweak_is_not_part_of_the_version(self):
        with self.assertRaisesRegex(ValueError, "VERSION_TWEAK"):
            read_version(version_file(tweak=1))

    def test_only_dev_or_nothing_as_extraversion(self):
        for extra in ("rc1", "rc.1", "beta", "dev.1"):
            with self.subTest(extra), self.assertRaisesRegex(ValueError, "EXTRAVERSION"):
                read_version(version_file(extra=extra))

    def test_no_leading_zeros(self):
        with self.assertRaisesRegex(ValueError, "SemVer"):
            read_version(version_file(minor="03"))

    def test_every_field_is_required(self):
        with self.assertRaisesRegex(ValueError, "PATCHLEVEL"):
            read_version("VERSION_MAJOR = 0\nVERSION_MINOR = 1\nVERSION_TWEAK = 0\nEXTRAVERSION = dev\n")

    def test_the_real_version_file(self):
        text = (Path(__file__).resolve().parents[2] / "VERSION").read_text()
        self.assertEqual(check(read_version(text), ""), [])


class Refs(unittest.TestCase):
    def test_any_branch_takes_dev_or_a_release(self):
        # main holds X.Y.Z between a release PR and the next -dev bump.
        for version in ("0.3.0-dev", "0.3.0"):
            for ref in ("", "refs/heads/main", "feat/42-semver-2-versions"):
                with self.subTest(version=version, ref=ref):
                    self.assertEqual(check(version, ref), [])

    def test_a_release_pr_sets_its_version(self):
        self.assertEqual(check("0.3.0", "release/v0.3.0"), [])
        self.assertEqual(check("0.3.0", "refs/heads/release/v0.3.0"), [])
        self.assertTrue(check("0.3.0-dev", "release/v0.3.0"))
        self.assertTrue(check("0.4.0", "release/v0.3.0"))

    def test_a_release_branch_names_a_stable_release(self):
        for ref in ("release/v0.3", "release/0.3.0", "release/v0.3.0-rc.1"):
            with self.subTest(ref):
                self.assertTrue(check("0.3.0", ref))

    def test_a_stable_tag_matches_version(self):
        self.assertEqual(check("0.3.0", "refs/tags/v0.3.0"), [])
        self.assertTrue(check("0.3.0-dev", "refs/tags/v0.3.0"))
        self.assertTrue(check("0.3.1", "refs/tags/v0.3.0"))

    def test_a_release_candidate_is_tagged_on_the_dev_version(self):
        self.assertEqual(check("0.3.0-dev", "refs/tags/v0.3.0-rc.1"), [])
        self.assertEqual(check("0.3.0-dev", "refs/tags/v0.3.0-rc.12"), [])
        self.assertTrue(check("0.4.0-dev", "refs/tags/v0.3.0-rc.1"))
        self.assertTrue(check("0.3.0", "refs/tags/v0.3.0-rc.1"))

    def test_only_allowed_tags(self):
        for tag in ("v0.3.0-rc1", "v0.3.0-rc.0", "v0.3.0-rc.01", "v0.3.0+1", "v0.3", "0.3.0", "v0.3.0-beta.1"):
            with self.subTest(tag):
                self.assertTrue(check("0.3.0-dev", f"refs/tags/{tag}"))


if __name__ == "__main__":
    unittest.main()
