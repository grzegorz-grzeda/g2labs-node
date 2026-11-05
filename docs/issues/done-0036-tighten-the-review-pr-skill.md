---
number: 36
title: "Tighten the review-pr skill"
status: closed
closed_reason: completed
closed: 2026-11-04
labels: [ready-for-agent]
parent:
blocked_by: []
assignee: claude
created: 2026-11-04
---

# Tighten the review-pr skill

## What to build

Follow-ups from the `claude-review` rounds on PRs #31 and #32 (#33's skill), all non-blocking there.

## Acceptance criteria

- [x] `SKILL.md` names the Standards/Spec review skill unambiguously (`mattpocock-skills:code-review`), not the built-in bug-finding `code-review`
- [x] `record.py finish` sets the `claude-review` status before posting the round comment, so a failed status write never leaves a counted round without a status
- [x] The `ready-for-human` decision is a small function covered by the unit tests
- [x] #35's closing comment names PR #32 as the live test

## Comments

### 2026-11-04 — claude

Done.
- `SKILL.md` names `mattpocock-skills:code-review` and says not to use the built-in `code-review`. It also allows reviewing both axes by hand for small diffs, which is what the reviewers did on PRs #31 and #32.
- `record.py finish` now sets the status before posting the comment.
- `should_escalate()` decides on `ready-for-human`; the comment text and `finish` both use it, and it has its own tests.
- #35's closing comment now names PR #32.
