---
number: 41
title: "Prefix closed issue files with done-"
status: closed
closed_reason: completed
closed: 2026-11-05
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

- [x] `docs/agents/issue-tracker.md` states the rule: closing an issue renames its file to `done-NNNN-slug.md` (`git mv`), whatever the `closed_reason`
- [x] Everything that finds an issue file by its name finds both forms: `CLAUDE.md`, the tracker conventions, the `review-pr` skill and the `open-tickets` script
- [x] The `open-tickets` tests check that a file has the `done-` prefix exactly when its issue is closed
- [x] Every closed issue in `docs/issues/` is renamed

## Comments

### 2026-11-05 — claude

The rule is in `docs/agents/issue-tracker.md` (Close, Read, List, Create), `CLAUDE.md` and the `review-pr` skill. `open-tickets` loads both file forms, and its tests fail whenever a file's `done-` prefix and its `status` disagree. The 23 issues that were already closed are renamed, `not_planned` ones (#11, #12, #13) included, so that the prefix always matches `status: closed`. This issue is the first one closed under the rule.
