#!/usr/bin/env python3
"""The human-gate verdict for a PR.

A PR passes unless it changes a gated path (.github/gated-paths) or carries
`ready-for-human`; then it passes only with `human-approved`.

  human_gate.py --patterns .github/gated-paths --labels a,b < changed-files

Prints the verdict and its reason; exits 0 to pass, 1 to fail.
"""

import argparse
import re
import sys
from pathlib import Path

APPROVED = "human-approved"
NEEDS_HUMAN = "ready-for-human"


def load_patterns(text):
    return [
        line.strip()
        for line in text.splitlines()
        if line.strip() and not line.lstrip().startswith("#")
    ]


def glob_regex(pattern):
    """`**/` and a trailing `/**` span directories, `*` and `?` don't cross `/`."""
    out = []
    i = 0
    while i < len(pattern):
        if pattern.startswith("**/", i):
            out.append("(?:.*/)?")
            i += 3
        elif pattern.startswith("**", i):
            out.append(".*")
            i += 2
        elif pattern[i] == "*":
            out.append("[^/]*")
            i += 1
        elif pattern[i] == "?":
            out.append("[^/]")
            i += 1
        else:
            out.append(re.escape(pattern[i]))
            i += 1
    return re.compile("".join(out) + r"\Z")


def gated_files(files, patterns):
    regexes = [glob_regex(p) for p in patterns]
    return [f for f in files if any(r.match(f) for r in regexes)]


def verdict(files, labels, patterns):
    """Returns (passed, reason)."""
    gated = gated_files(files, patterns)
    reasons = []
    if gated:
        shown = ", ".join(gated[:3]) + (f" and {len(gated) - 3} more" if len(gated) > 3 else "")
        reasons.append(f"changes gated paths ({shown})")
    if NEEDS_HUMAN in labels:
        reasons.append(f"is labelled {NEEDS_HUMAN}")
    if not reasons:
        return True, "no gated paths"
    if APPROVED in labels:
        return True, f"{APPROVED}: " + " and ".join(reasons)
    return False, "waiting for human-approved: " + " and ".join(reasons)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--patterns", type=Path, required=True)
    parser.add_argument("--labels", default="", help="comma-separated PR labels")
    args = parser.parse_args()
    files = [line.strip() for line in sys.stdin if line.strip()]
    labels = {l.strip() for l in args.labels.split(",") if l.strip()}
    passed, reason = verdict(files, labels, load_patterns(args.patterns.read_text()))
    print(reason)
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
