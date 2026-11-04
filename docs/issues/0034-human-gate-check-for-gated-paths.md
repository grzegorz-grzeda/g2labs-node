---
number: 34
title: "human-gate check for gated paths"
status: open
labels: [ready-for-agent]
parent:
blocked_by: []
created: 2026-11-04
---

# human-gate check for gated paths

## What to build

A CI job, `human-gate`, that keeps a PR from auto-merging while it changes something the maintainer must see (the gated paths in `docs/agents/development-flow.md`), until the maintainer adds the `human-approved` label.

## Acceptance criteria

- [ ] Runs on `pull_request` (opened, synchronize, labeled, unlabeled)
- [ ] Fails when the PR touches `.github/workflows/**`, `sysbuild*`, MCUboot key settings, `contract/**`, `docs/adr/**` or `VERSION`, or carries `ready-for-human`, unless it carries `human-approved`
- [ ] Passes otherwise, so it can be a required check on every PR
- [ ] A push after `human-approved` removes the label, so approval covers only what was looked at
- [ ] The gated path list lives in one place, shared with the flow doc
