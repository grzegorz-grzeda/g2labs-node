---
number: 44
title: "Authenticate the Zephyr SDK install in CI"
status: closed
closed_reason: completed
closed: 2026-11-19
labels: []
parent:
blocked_by: []
created: 2026-11-19
---

# Authenticate the Zephyr SDK install in CI

## What to build

On a cache miss, `west sdk install` looks up the SDK release through the GitHub API without a token, so it shares the runner IP's 60 requests an hour and fails with 403 when they run out (CI run 44564852686, for 2e67fb3). Pass the workflow's token with `--personal-access-token`.

## Acceptance criteria

- [x] `ci.yml`'s SDK install step passes `github.token`

## Comments

### 2026-11-19 — claude

Done: the step passes the token through an env var, not inline in the command.
