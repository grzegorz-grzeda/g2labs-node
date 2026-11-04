#!/usr/bin/env python3
"""Records a claude-review round on a GitHub PR.

  record.py start PR
      Sets `claude-review` pending on the PR's head commit and prints
      `<head-sha> <round>`. Run it before reviewing; review exactly that SHA.

  record.py finish PR --sha SHA --verdict success|failure --report FILE
      Posts FILE as the round's PR comment, sets `claude-review` on SHA and,
      on the third failed round, adds `ready-for-human`. Refuses if the PR's
      head is no longer SHA (a new push needs a new review).

Rounds are counted from the marker comments earlier rounds left on the PR,
so the count is visible on the PR itself and needs no other state.
"""

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

CONTEXT = "claude-review"
NEEDS_HUMAN = "ready-for-human"
MAX_FAILED_ROUNDS = 3
MARKER = re.compile(r"<!-- claude-review round=(\d+) sha=([0-9a-f]+) verdict=(success|failure) -->")


def past_rounds(comment_bodies):
    """[(round, sha, verdict)] from earlier rounds' comments, in order."""
    rounds = []
    for body in comment_bodies:
        m = MARKER.search(body)
        if m:
            rounds.append((int(m.group(1)), m.group(2), m.group(3)))
    return rounds


def next_round(rounds):
    return max((r for r, _, _ in rounds), default=0) + 1


def failed_rounds(rounds):
    return sum(1 for _, _, v in rounds if v == "failure")


def render_comment(round_no, sha, verdict, report, failures):
    icon = "✅ passed" if verdict == "success" else "❌ changes needed"
    lines = [
        f"<!-- claude-review round={round_no} sha={sha} verdict={verdict} -->",
        f"## claude-review, round {round_no}: {icon}",
        "",
        f"Reviewed `{sha[:12]}` with fresh context (Standards and Spec).",
        "",
        report.strip(),
    ]
    if verdict == "failure" and failures >= MAX_FAILED_ROUNDS:
        lines += [
            "",
            f"This is failed round {failures}, so the PR is labelled `{NEEDS_HUMAN}` "
            "and stops here until the maintainer looks at it.",
        ]
    return "\n".join(lines) + "\n"


def status_description(round_no, verdict):
    return f"Round {round_no}: " + ("passed" if verdict == "success" else "changes needed")


# --- GitHub, through the gh CLI -------------------------------------------


def gh(*args, input=None):
    return subprocess.run(["gh", *args], check=True, capture_output=True, text=True, input=input).stdout


def repo():
    return gh("repo", "view", "--json", "nameWithOwner", "--jq", ".nameWithOwner").strip()


def head_sha(r, pr):
    return gh("api", f"repos/{r}/pulls/{pr}", "--jq", ".head.sha").strip()


def comment_bodies(r, pr):
    out = gh("api", "--paginate", f"repos/{r}/issues/{pr}/comments", "--jq", ".[].body | @json")
    return [json.loads(line) for line in out.splitlines() if line.strip()]


def set_status(r, sha, state, description, pr):
    gh(
        "api", f"repos/{r}/statuses/{sha}",
        "-f", f"state={state}",
        "-f", f"context={CONTEXT}",
        "-f", f"description={description}",
        "-f", f"target_url=https://github.com/{r}/pull/{pr}",
    )


def cmd_start(args):
    r = repo()
    sha = head_sha(r, args.pr)
    round_no = next_round(past_rounds(comment_bodies(r, args.pr)))
    set_status(r, sha, "pending", f"Round {round_no}: reviewing", args.pr)
    print(sha, round_no)
    return 0


def cmd_finish(args):
    r = repo()
    current = head_sha(r, args.pr)
    if current != args.sha:
        print(f"PR #{args.pr} moved to {current[:12]} since the review of {args.sha[:12]} started; "
              "nothing recorded. Review the new head.", file=sys.stderr)
        return 2
    rounds = past_rounds(comment_bodies(r, args.pr))
    round_no = next_round(rounds)
    failures = failed_rounds(rounds) + (args.verdict == "failure")
    body = render_comment(round_no, args.sha, args.verdict, args.report.read_text(), failures)
    gh("pr", "comment", str(args.pr), "--body-file", "-", input=body)
    set_status(r, args.sha, args.verdict, status_description(round_no, args.verdict), args.pr)
    if args.verdict == "failure" and failures >= MAX_FAILED_ROUNDS:
        gh("pr", "edit", str(args.pr), "--add-label", NEEDS_HUMAN)
    print(f"round {round_no}: {args.verdict}" + (f", labelled {NEEDS_HUMAN}" if failures >= MAX_FAILED_ROUNDS and args.verdict == "failure" else ""))
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="cmd", required=True)
    start = sub.add_parser("start")
    start.add_argument("pr", type=int)
    finish = sub.add_parser("finish")
    finish.add_argument("pr", type=int)
    finish.add_argument("--sha", required=True)
    finish.add_argument("--verdict", choices=["success", "failure"], required=True)
    finish.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    return cmd_start(args) if args.cmd == "start" else cmd_finish(args)


if __name__ == "__main__":
    sys.exit(main())
