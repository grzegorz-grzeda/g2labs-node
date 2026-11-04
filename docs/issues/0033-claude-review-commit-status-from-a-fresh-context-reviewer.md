---
number: 33
title: "claude-review commit status from a fresh-context reviewer"
status: open
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

- [ ] A project skill (e.g. `.claude/skills/review-pr/`) takes a PR number, reviews its diff against `main` for Standards and Spec, posts the findings as one PR comment and sets the `claude-review` status (`success`/`failure`) on the PR's head commit via `gh api`
- [ ] The reviewer never sees the author's session: it starts from the PR, its linked issue and the repo
- [ ] A new push to the PR leaves `claude-review` pending until the reviewer runs on the new head
- [ ] The review round count is visible on the PR, and the third failure adds `ready-for-human`
- [ ] `docs/agents/development-flow.md` says how the author invokes it
