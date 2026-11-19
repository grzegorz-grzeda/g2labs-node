#!/usr/bin/env python3
"""Check VERSION, and the git ref being built, against the versioning rule.

Firmware versions are SemVer 2.0.0 without build metadata
(docs/agents/development-flow.md, Versioning):

  X.Y.Z-dev     main between Releases (VERSION's EXTRAVERSION = dev)
  X.Y.Z-rc.N    a Release Candidate, tagged vX.Y.Z-rc.N on a main commit
                whose VERSION is X.Y.Z-dev
  X.Y.Z         a stable Release: VERSION in its release commit, tagged vX.Y.Z

  check_version.py [--version-file VERSION] [--ref REF]

REF is a tag (refs/tags/…), a branch or empty; only tags are checked. Prints what is wrong; exits 0 when nothing is, 1 otherwise.
"""

import argparse
import re
import sys
from pathlib import Path

# The SemVer 2.0.0 grammar (semver.org), without build metadata.
SEMVER = re.compile(
    r"(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)"
    r"(?:-((?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*)(?:\.(?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*))*))?"
)
RELEASE = re.compile(r"(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)")
CANDIDATE = re.compile(RELEASE.pattern + r"-rc\.[1-9]\d*")

FIELDS = ("VERSION_MAJOR", "VERSION_MINOR", "PATCHLEVEL", "VERSION_TWEAK", "EXTRAVERSION")


def read_version(text):
    """The version VERSION describes, e.g. 0.3.0-dev; ValueError if not allowed."""
    values = {}
    for field in FIELDS:
        m = re.search(rf"^{field} = ?(.*?)\s*$", text, re.M)
        if m is None:
            raise ValueError(f"VERSION has no {field}")
        values[field] = m.group(1)
    if values["VERSION_TWEAK"] != "0":
        raise ValueError("VERSION_TWEAK must be 0: it is not part of the version")
    if values["EXTRAVERSION"] not in ("", "dev"):
        raise ValueError(
            f"EXTRAVERSION must be dev (main) or empty (a release commit), not {values['EXTRAVERSION']!r}"
        )
    core = ".".join(values[f] for f in FIELDS[:3])
    version = core + ("-" + values["EXTRAVERSION"] if values["EXTRAVERSION"] else "")
    if not SEMVER.fullmatch(version):
        raise ValueError(f"{version} is not a SemVer 2.0.0 version")
    return version


def check(version, ref):
    """What is wrong with building `version` (from read_version) at `ref`."""
    if ref.startswith("refs/tags/"):
        tag = ref.removeprefix("refs/tags/")
        if not tag.startswith("v"):
            return [f"tag {tag} must be v followed by the version"]
        tagged = tag[1:]
        if RELEASE.fullmatch(tagged):
            if version != tagged:
                return [f"tag {tag} is a stable Release, so VERSION must be {tagged}, not {version}"]
            return []
        if CANDIDATE.fullmatch(tagged):
            dev = tagged.split("-")[0] + "-dev"
            if version != dev:
                return [f"tag {tag} is a Release Candidate, so it goes on a commit whose VERSION is {dev}, not {version}"]
            return []
        return [f"tag {tag} is neither vX.Y.Z nor vX.Y.Z-rc.N (N from 1)"]

    return []


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--version-file", type=Path, default=Path(__file__).resolve().parents[2] / "VERSION")
    parser.add_argument("--ref", default="")
    args = parser.parse_args()
    try:
        version = read_version(args.version_file.read_text())
    except ValueError as e:
        print(e)
        return 1
    errors = check(version, args.ref)
    for error in errors:
        print(error)
    if not errors:
        print(f"version {version} ok" + (f" for {args.ref}" if args.ref else ""))
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
