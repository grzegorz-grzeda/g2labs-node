---
number: 35
title: "Protect main and turn on auto-merge"
status: open
labels: [ready-for-human]
parent:
blocked_by: [33, 34]
created: 2026-11-04
---

# Protect main and turn on auto-merge

## What to build

The GitHub settings the development flow relies on. Blocked until `claude-review` and `human-gate` exist, since a required check that never reports blocks every merge. Repository settings change only with the maintainer's go-ahead.

## Acceptance criteria

- [ ] Repository: squash merge only, PR title as the commit subject, "Allow auto-merge" on, branches deleted on merge
- [ ] `main` protection: PR required, required checks = every CI job plus `claude-review` and `human-gate`, branch must be up to date, no force-push, no deletion
- [ ] Admin bypass stays on for emergencies
- [ ] Labels `ready-for-human` and `human-approved` exist on GitHub
- [ ] A throwaway PR shows auto-merge waiting for each check and merging when all pass
