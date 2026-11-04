---
number: 34
title: "human-gate check for gated paths"
status: closed
closed_reason: completed
closed: 2026-11-04
labels: [ready-for-agent]
parent:
blocked_by: []
assignee: claude
created: 2026-11-04
---

# human-gate check for gated paths

## What to build

A CI job, `human-gate`, that keeps a PR from auto-merging while it changes something the maintainer must see (the gated paths in `docs/agents/development-flow.md`), until the maintainer adds the `human-approved` label.

## Acceptance criteria

- [x] Runs on `pull_request` (opened, synchronize, labeled, unlabeled)
- [x] Fails when the PR touches `.github/workflows/**`, `sysbuild*`, MCUboot key settings, `contract/**`, `docs/adr/**` or `VERSION`, or carries `ready-for-human`, unless it carries `human-approved`
- [x] Passes otherwise, so it can be a required check on every PR
- [x] A push after `human-approved` removes the label, so approval covers only what was looked at
- [x] The gated path list lives in one place, shared with the flow doc

## Comments

### 2026-11-04 — claude

Done. `.github/workflows/human-gate.yml` runs on `pull_request_target` (opened, synchronize, reopened, labeled, unlabeled), always from `main`'s copy, so a PR can't change its own gate. It reads the changed files (including the old names of renamed files) and the labels from the API, without checking out PR code. It posts the verdict as the `human-gate` commit status on the PR's head commit, which is what branch protection (#35) requires. On a push it removes `human-approved` and says so in a comment.

The gated paths are listed only in `.github/gated-paths`, which the flow doc links to. Besides the paths in this issue, the list also covers the gate's own files and the reviewer skill (`.claude/skills/review-pr/**`). The matching logic is in `.github/scripts/human_gate.py`, and its unit tests run in CI's `build-and-test` job.

The gate can't run on the PR that adds it, because `pull_request_target` uses `main`'s workflows. Its first real run is on the next PR.
