---
number: 36
title: "Tighten the review-pr skill"
status: open
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

- [ ] `SKILL.md` names the Standards/Spec review skill unambiguously (`mattpocock-skills:code-review`), not the built-in bug-finding `code-review`
- [ ] `record.py finish` sets the `claude-review` status before posting the round comment, so a failed status write never leaves a counted round without a status
- [ ] The `ready-for-human` decision is a small function covered by the unit tests
- [ ] #35's closing comment names PR #32 as the live test
