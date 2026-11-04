#!/usr/bin/env python3
"""Lists the open, unblocked tickets in docs/issues/.

  tickets.py [--issues DIR] [--all] [--json]

A ticket is unblocked when every issue in its `blocked_by` is closed. Specs
(open issues that other issues name as their `parent`) are listed apart,
because they close when their children do, not by being worked on.

For each ticket it prints the number, title, labels, parent, assignee (if
claimed), how many open tickets it blocks, and the first paragraph of its
"What to build" section as a summary. --all also lists blocked tickets with
their open blockers. --json prints the same data as JSON.
"""

import argparse
import json
import re
import sys
from pathlib import Path

DEFAULT_DIR = Path(__file__).resolve().parents[3] / "docs" / "issues"


def parse_value(raw):
    raw = raw.strip()
    if raw.startswith("[") and raw.endswith("]"):
        return [parse_value(x) for x in raw[1:-1].split(",") if x.strip()]
    if len(raw) >= 2 and raw[0] == raw[-1] == '"':
        return raw[1:-1]
    if re.fullmatch(r"-?\d+", raw):
        return int(raw)
    return raw


def parse_issue(text):
    """Front matter fields plus `summary`, or None if the file has none."""
    m = re.match(r"---\n(.*?)\n---\n(.*)", text, re.S)
    if not m:
        return None
    fields = {}
    for line in m.group(1).splitlines():
        key, sep, value = line.partition(":")
        if sep:
            fields[key.strip()] = parse_value(value)
    fields["summary"] = summary(m.group(2))
    return fields


def summary(body):
    """The first paragraph under "## What to build", on one line."""
    m = re.search(r"^## What to build\s*\n+(.*?)(?:\n\s*\n|\n#|\Z)", body, re.S | re.M)
    return " ".join(m.group(1).split()) if m else ""


def load(issues_dir):
    issues = {}
    for path in sorted(Path(issues_dir).glob("[0-9][0-9][0-9][0-9]-*.md")):
        issue = parse_issue(path.read_text())
        if issue and isinstance(issue.get("number"), int):
            issue["file"] = path.name
            issues[issue["number"]] = issue
    return issues


def as_list(value):
    if value in (None, ""):
        return []
    return value if isinstance(value, list) else [value]


def is_open(issues, number):
    """Unknown issue numbers count as open, so a typo never unblocks."""
    issue = issues.get(number)
    return issue is None or issue.get("status") != "closed"


def tickets(issues):
    """Open issues with their open blockers, how many they block, and kind."""
    # A spec is any issue that has children, open or closed.
    parents = {i.get("parent") for i in issues.values()}
    rows = []
    for n, issue in sorted(issues.items()):
        if not is_open(issues, n):
            continue
        blockers = [b for b in as_list(issue.get("blocked_by")) if is_open(issues, b)]
        blocks = [
            m for m, other in issues.items()
            if is_open(issues, m) and n in as_list(other.get("blocked_by"))
        ]
        rows.append({
            "number": n,
            "title": issue.get("title", ""),
            "labels": as_list(issue.get("labels")),
            "parent": issue.get("parent") or None,
            "assignee": issue.get("assignee") or None,
            "kind": "spec" if n in parents else "ticket",
            "open_blockers": blockers,
            "blocks": sorted(blocks),
            "summary": issue.get("summary", ""),
            "file": issue["file"],
        })
    return rows


def entry(r):
    meta = []
    if r["labels"]:
        meta.append(", ".join(r["labels"]))
    if r["parent"]:
        meta.append(f"parent #{r['parent']}")
    if r["assignee"]:
        meta.append(f"assignee {r['assignee']}")
    if r["blocks"]:
        meta.append("blocks " + " ".join(f"#{b}" for b in r["blocks"]))
    if r["open_blockers"]:
        meta.append("blocked by " + " ".join(f"#{b}" for b in r["open_blockers"]))
    lines = [f"#{r['number']} {r['title']}"]
    if meta:
        lines.append(f"  [{'; '.join(meta)}]")
    if r["summary"]:
        lines.append(f"  {r['summary']}")
    return "\n".join(lines)


def section(heading, rows):
    return heading + "\n\n" + ("\n\n".join(entry(r) for r in rows) if rows else "(none)")


def render(rows, show_blocked):
    ready = [r for r in rows if r["kind"] == "ticket" and not r["open_blockers"]]
    blocked = [r for r in rows if r["kind"] == "ticket" and r["open_blockers"]]
    specs = [r for r in rows if r["kind"] == "spec"]
    parts = [section(f"Unblocked tickets ({len(ready)}):", ready)]
    if show_blocked:
        parts.append(section(f"Blocked tickets ({len(blocked)}):", blocked))
    else:
        parts.append(f"{len(blocked)} blocked ticket(s) not shown (--all lists them).")
    parts.append(f"Open specs ({len(specs)}): " + (", ".join(f"#{r['number']} {r['title']}" for r in specs) or "none"))
    return "\n\n".join(parts) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--issues", type=Path, default=DEFAULT_DIR)
    parser.add_argument("--all", action="store_true", help="also list blocked tickets")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()
    rows = tickets(load(args.issues))
    if args.json:
        if not args.all:
            rows = [r for r in rows if r["kind"] == "spec" or not r["open_blockers"]]
        json.dump(rows, sys.stdout, indent=2)
        print()
    else:
        sys.stdout.write(render(rows, args.all))
    return 0


if __name__ == "__main__":
    sys.exit(main())
