---
number: 33
title: "claude-review commit status from a fresh-context reviewer"
status: closed
closed_reason: completed
closed: 2026-11-04
labels: [ready-for-agent]
parent:
blocked_by: []
assignee: claude
created: 2026-11-04
---

# claude-review commit status from a fresh-context reviewer

## What to build

The review step of `docs/agents/development-flow.md` as something repeatable: after opening a PR, the authoring session hands it to a reviewer with fresh context, which runs the `code-review` skill against the linked issue and records the outcome on the PR as the `claude-review` commit status.

## Acceptance criteria

- [x] A project skill (e.g. `.claude/skills/review-pr/`) takes a PR number, reviews its diff against `main` for Standards and Spec, posts the findings as one PR comment and sets the `claude-review` status (`success`/`failure`) on the PR's head commit via `gh api`
- [x] The reviewer never sees the author's session: it starts from the PR, its linked issue and the repo
- [x] A new push to the PR leaves `claude-review` pending until the reviewer runs on the new head
- [x] The review round count is visible on the PR, and the third failure adds `ready-for-human`
- [x] `docs/agents/development-flow.md` says how the author invokes it

## Comments

### 2026-11-04 — claude

Done. The project skill `.claude/skills/review-pr/` takes a PR number and pins the head SHA with `record.py start`, which also sets `claude-review` to pending. It reviews that SHA in a temporary worktree, running the `code-review` skill (Standards and Spec) against `origin/main` with the PR's linked issue as the spec. It then records the result with `record.py finish`: one PR comment headed "claude-review, round N", plus the `claude-review` commit status on the reviewed SHA. If the PR moved during the review, `finish` refuses and records nothing.

- **Round count**: hidden markers in the round comments carry the count, so it's visible on the PR and needs no other state. On the third failed round the reviewer adds `ready-for-human`.
- **New pushes**: commit statuses belong to one commit, so a new push leaves `claude-review` "Expected" on the new head until the reviewer runs again. Once #35 makes it a required check, that blocks merging.
- **Fresh context**: the skill tells the reviewer to work only from the PR, its issue and the repo. `docs/agents/development-flow.md` tells the author to start it from a non-fork subagent or a new session, with only `Use the review-pr skill on PR #<N>.`
- **Gating**: the skill is listed in `.github/gated-paths` (#34), so changes to the reviewer wait for the maintainer.
- **Tests**: the round logic's tests run in CI's `build-and-test` job.
