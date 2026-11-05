---
number: 38
title: "Let test-only CI changes merge without the maintainer"
status: open
labels: [ready-for-agent]
parent:
blocked_by: []
created: 2026-11-05
---

# Let test-only CI changes merge without the maintainer

## What to build

Today every change under `.github/workflows/**` waits for `human-approved`, so a PR that only adds a test job to `ci.yml` (GitHub PRs 34 and 36, for #37 and #14) waits for the maintainer like one that touches publishing or the gate. Narrow the gate so `ci.yml` merges on CI, `claude-review` and `human-gate` alone, while anything that can publish, hold secrets or write to the repo still waits for the maintainer.

- **Split publishing out of `ci.yml`.** Move `hub-image` (the `v*`-tag job that pushes to ghcr with `packages: write`) into a new `.github/workflows/release.yml`. #20's release job builds on it later. `ci.yml` keeps only building and testing. `hub-image` currently `needs: [build-and-test, hub]`. It has to keep publishing only a tag whose tests passed. If `release.yml` calls `ci.yml` as a reusable workflow (`workflow_call`), the called workflow can use whatever token permissions the calling job has. So the calling job must be read-only (`permissions: contents: read`, no `secrets: inherit`), and only a separate publish job that `needs` it holds `packages: write`. `ci.yml`'s own `push` trigger then skips tags (`branches: ['**']`), so a tag doesn't run the tests twice.
- **Exclusions in `gated-paths`.** `human_gate.py` learns `!pattern` lines: a file matching a later `!` line is not gated. Every workflow stays gated by default (`.github/workflows/**`), with `!.github/workflows/ci.yml` as the one exception, so a new workflow file still waits for the maintainer.
- **Guard `ci.yml`'s contents.** Ungated, `ci.yml` could grant itself power that would run on a same-repo PR before anyone looked. When a PR changes `ci.yml`, `human-gate` fetches the PR's version through the API (as data, never checked out or run) and treats it as gated unless it keeps its top-level `permissions: contents: read`, and it is also gated if it contains any of: a `permissions:` entry (at any level) other than `read`, `none` or `read-all`, including `write-all`; `secrets.` or `secrets: inherit`; `pull_request_target` or `workflow_run`. If the fetch fails, the gate fails.
- **Docs.** `docs/agents/development-flow.md` (Required checks) describes the narrower gate and the `ci.yml` content rule. CLAUDE.md's CI section names `release.yml`.

Removing a CI job stays safe without the gate: every job is a required check on `main`, so a PR that drops one waits on "Expected" forever. Weakening a job's steps is for `claude-review` to catch.

Out of scope: the contract (`contract/**` stops being gated in #31), ADRs, `sysbuild*`/`*.pem`, `VERSION`, the gate's own files and the `review-pr` skill, which all stay gated. ADR-0006 is unchanged.

## Acceptance criteria

- [ ] `hub-image` lives in `release.yml`, still runs only on `v*` tags, only after the tag's build and tests pass, and pushes the same tags as before; `ci.yml` has no job with write permissions or secrets
- [ ] If `release.yml` calls `ci.yml`, the calling job is read-only with no `secrets: inherit`, and `packages: write` is held only by the separate publish job; a tag runs the tests once
- [ ] `human_gate.py` supports `!pattern` lines (last match wins), with unit tests
- [ ] `gated-paths` gates `.github/workflows/**` except `ci.yml`
- [ ] A PR changing `ci.yml` is gated when its `ci.yml` drops the top-level `permissions: contents: read`, grants a non-read permission (including `write-all`) at any level, or uses `secrets.`, `secrets: inherit`, `pull_request_target` or `workflow_run`, and passes otherwise; unit tests cover each case and a failed fetch fails the gate
- [ ] `development-flow.md` and CLAUDE.md describe the new gate and `release.yml`
- [ ] This PR itself needs `human-approved` (it changes the gate); the first later PR that only touches `ci.yml` merges without it
