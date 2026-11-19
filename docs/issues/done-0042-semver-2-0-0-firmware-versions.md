---
number: 42
title: "SemVer 2.0.0 firmware versions"
status: closed
closed_reason: completed
closed: 2026-11-05
labels: [ready-for-agent]
assignee: claude
parent:
blocked_by: []
created: 2026-11-05
---

# SemVer 2.0.0 firmware versions

## What to build

Firmware versions follow SemVer 2.0.0 exactly, so that the Hub can later compare them by SemVer precedence (Follow Releases, #29; checking an update, #28). Today a Node reports `0.1.0-dev+0`, which carries Zephyr's `VERSION_TWEAK` as build metadata, and a Release Candidate is tagged `vX.Y.Z-rcN`, which SemVer sorts wrongly from `rc10` on (`rc10` < `rc2`).

## Acceptance criteria

- [x] `docs/agents/development-flow.md` states the rule: which versions exist (`X.Y.Z-dev` on `main`, `X.Y.Z-rc.N` for a Release Candidate, `X.Y.Z` for a stable Release), how tags are named (`v` + the version), that versions are compared by SemVer 2.0.0 precedence, and that no version carries build metadata
- [x] A Node reports its version without build metadata: `fw_version` and `node version` read `0.1.0-dev`, not `0.1.0-dev+0`
- [x] Release Candidates are named `vX.Y.Z-rc.N` everywhere they are mentioned (the flow, `CLAUDE.md`, `ci.yml`, #20)
- [x] A script with tests, run in CI, rejects a `VERSION` that doesn't form an allowed version, a release PR (`release/vX.Y.Z`) whose `VERSION` isn't `X.Y.Z`, and a `v*` tag that isn't an allowed version or doesn't match `VERSION`

## Comments

### 2026-11-05 — claude

The rule is in `docs/agents/development-flow.md` (Versioning): three shapes (`X.Y.Z-dev`, `X.Y.Z-rc.N`, `X.Y.Z`), tags `v` + the version, SemVer 2.0.0 precedence for every comparison, no build metadata, `VERSION_TWEAK` 0. A Node now reports `APP_VERSION_STRING`: the ESP32-C6 on the bench says `0.1.0-dev`, stays connected to its Broker, and its image confirmed itself. MCUboot still signs `0.1.0+0`. `.github/scripts/check_version.py` (18 tests) runs in `build-and-test` with the PR's head branch or the pushed ref. Release Candidates are `rc.N` in the flow, `CLAUDE.md`, `ci.yml` and #20, which now also builds a candidate's images with `EXTRAVERSION = rc.N` so that they report their own version.
