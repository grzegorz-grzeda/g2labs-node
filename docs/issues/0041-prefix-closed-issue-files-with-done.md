---
number: 41
title: "Prefix closed issue files with done-"
status: open
labels: [ready-for-agent]
assignee: claude
parent:
blocked_by: []
created: 2026-11-05
---

# Prefix closed issue files with done-

## What to build

Telling finished issues from live ones in `docs/issues/` takes reading each file's front matter. Make it visible in the file name instead: a closed issue's file is renamed to `done-NNNN-slug.md` in the same commit that closes it, and every existing closed issue is renamed now.

## Acceptance criteria

- [ ] `docs/agents/issue-tracker.md` states the rule: closing an issue renames its file to `done-NNNN-slug.md` (`git mv`), whatever the `closed_reason`
- [ ] Everything that finds an issue file by its name finds both forms: `CLAUDE.md`, the tracker conventions, the `review-pr` skill and the `open-tickets` script
- [ ] The `open-tickets` tests check that a file has the `done-` prefix exactly when its issue is closed
- [ ] Every closed issue in `docs/issues/` is renamed
