---
number: 37
title: "open-tickets project skill"
status: closed
closed_reason: completed
closed: 2026-11-04
labels: [ready-for-agent]
parent:
blocked_by: []
assignee: claude
created: 2026-11-04
---

# open-tickets project skill

## What to build

A project skill that lists and summarizes the open, unblocked tickets in `docs/issues/`, so "what can be worked on now" has one repeatable answer.

## Acceptance criteria

- [x] `.claude/skills/open-tickets/` lists open tickets whose `blocked_by` issues are all closed, with labels, parent, assignee, what each one blocks and a one-line summary
- [x] Specs (open parents) are listed apart; blocked tickets are shown on request
- [x] The skill tells Claude how to summarize the list (grouped by spec, who can take each, what unblocks the most)
- [x] Tested, and the tests run in CI

## Comments

### 2026-11-04 — claude

Done. `tickets.py` parses the issue files' front matter. It treats an unknown blocker number as open, so a typo never unblocks a ticket. It takes the summary from the first paragraph of "What to build". `SKILL.md` says how to present the list. The tests use fixture issue files plus a check that every real issue file parses.

CI's "Development flow scripts' tests" step now runs the tests of every project skill in `.claude/skills/*/`, so a new skill's tests need no workflow change.
