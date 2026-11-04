---
number: 35
title: "Protect main and turn on auto-merge"
status: closed
closed_reason: completed
closed: 2026-11-04
labels: [ready-for-human]
parent:
blocked_by: [33, 34]
assignee: claude
created: 2026-11-04
---

# Protect main and turn on auto-merge

## What to build

The GitHub settings the development flow relies on. Blocked until `claude-review` and `human-gate` exist, since a required check that never reports blocks every merge. Repository settings change only with the maintainer's go-ahead.

## Acceptance criteria

- [x] Repository: squash merge only, PR title as the commit subject, "Allow auto-merge" on, branches deleted on merge
- [x] `main` protection: PR required, required checks = every CI job plus `claude-review` and `human-gate`, branch must be up to date, no force-push, no deletion
- [x] Admin bypass stays on for emergencies
- [x] Labels `ready-for-human` and `human-approved` exist on GitHub
- [x] A throwaway PR shows auto-merge waiting for each check and merging when all pass

## Comments

### 2026-11-04 — claude

Done, with the maintainer's go-ahead given in the session.

- **Repository**: squash merge only. The commit title is the PR title and the message is the PR body. Auto-merge is allowed and branches are deleted on merge.
- **`main` protection**: a PR is required (0 approving reviews, because the gate is `human-gate`, not a GitHub review). The branch must be up to date. Required checks:
  - `build-and-test` and `hub` (the `ci.yml` jobs that run on PRs; `hub-image` runs only on tags);
  - `human-gate`, accepted only from GitHub Actions, so no other token can set it;
  - `claude-review`, accepted from any source, because the reviewer sets it with the maintainer's `gh` login.
- **Pushes**: force-push and deletion are off. Admins aren't included (`enforce_admins: false`), so the admin bypass stays for emergencies.
- **Labels**: `ready-for-human` already existed; `human-approved` was added.
- **Live test**: this PR, opened with auto-merge on, waited for each check and merged itself once all of them passed.

Live runs while landing #33 and #34 showed the gate working. A push to PR #31 after approval removed `human-approved` and set `human-gate` to failure; adding the label again turned it to success.
