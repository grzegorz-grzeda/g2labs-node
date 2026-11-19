---
number: 47
title: "Write a spec and tickets after a design conversation, before implementing"
status: closed
closed_reason: completed
closed: 2026-11-19
labels: []
parent:
blocked_by: []
created: 2026-11-19
---

# Write a spec and tickets after a design conversation, before implementing

## What to build

After the grillings for #43 and #45, Claude went straight from "confirmed" to code, writing the issues as part of the implementation. The maintainer wants the outcome of a design conversation written up as a spec and tickets first, approved, and only then implemented.

## Acceptance criteria

- [x] `docs/agents/development-flow.md` says what follows a design conversation: spec, tickets, approval, then implementation
- [x] CLAUDE.md records that the core's API grows per ticket (until now only in Claude's local memory)

## Comments

### 2026-11-19 — claude

Done: a new section "From a design conversation to tickets" in `development-flow.md`, and the core-API-per-ticket rule in CLAUDE.md's `core/` entry, both of which had lived only in Claude's local memory.
