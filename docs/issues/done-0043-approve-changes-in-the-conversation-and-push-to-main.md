---
number: 43
title: "Approve changes in the conversation and push to main"
status: closed
closed_reason: completed
closed: 2026-11-19
labels: []
parent:
blocked_by: []
created: 2026-11-19
---

# Approve changes in the conversation and push to main

## What to build

The maintainer approves changes while talking to Claude, so PRs, `human-gate`, the `claude-review` check and the release PR only repeat that approval. Replace them with approval in the conversation and direct pushes to `main` (ADR-0007).

## Acceptance criteria

- [x] ADR-0007 records the decision; ADR-0006 points to it
- [x] `docs/agents/development-flow.md` and CLAUDE.md describe the new flow
- [x] The `human-gate` workflow, `.github/gated-paths` and `human_gate.py` with its tests are gone
- [x] The `review-pr` skill becomes `review-change`: a fresh-context review of a local branch, reported back to the caller, with no GitHub writes
- [x] `check_version.py` no longer knows release branches
- [x] #38 closed as not planned; #20 describes stable Releases from a pushed tag

## Comments

### 2026-11-19 — claude

Done in the conversation that decided it, as the first change pushed under the new rule. `main`'s branch protection (a GitHub setting) drops the required checks and the PR rule right after this push, keeping no force pushes and no deletion.
