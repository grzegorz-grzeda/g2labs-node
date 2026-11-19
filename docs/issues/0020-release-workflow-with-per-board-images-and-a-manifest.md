---
number: 20
title: "Release workflow with per-board images and a manifest"
status: open
labels: [ready-for-agent]
parent: 1
blocked_by: []
created: 2026-11-04
---

# Release workflow with per-board images and a manifest

## What to build

The Hub's Follow Releases Update Policy (#17) needs releases it can use: pushing a `v*` tag produces a GitHub Release with one signed image per board and a manifest that tells the Hub which image is for which board.

## Acceptance criteria

- [ ] A Release Candidate is published when a `v*-rc.N` tag is pushed; a stable Release when a `vX.Y.Z` tag is pushed on the release commit, which sets `VERSION` without `-dev` (see `docs/agents/development-flow.md`, Releases)
- [ ] Both build both boards through sysbuild, as CI does
- [ ] The release gets `g2labs-node-<version>-<board>.signed.bin` per board (board target with `/` replaced by `-`), the MCUboot-signed Node image suitable for SMP upload
- [ ] The release gets `manifest.json`: version plus, per board target, the asset name and its SHA-256
- [ ] The manifest has a JSON Schema in `contract/`
- [ ] The tag must match `VERSION`, or the workflow fails
- [ ] Release Candidates (e.g. `v0.3.0-rc.1`), built with `EXTRAVERSION = rc.N` in the build's copy of `VERSION` (never committed; `check_version.py` allows only `dev` or empty there) so their images report `X.Y.Z-rc.N` (`docs/agents/development-flow.md`, Versioning) produce GitHub pre-releases
- [ ] The same `v*` tag also publishes the Hub image (#22), only after that tag's build and tests pass

## Comments

### 2026-11-04 — claude

Reworked for the development flow (ADR-0006): stable Releases come from the maintainer merging a release PR, not from a pushed tag; Release Candidates still come from pushed tags.

### 2026-11-19 — claude

Reworked for ADR-0007: there is no release PR any more. Claude pushes the release commit and the `vX.Y.Z` tag after the maintainer approves the Release in the conversation, so a pushed tag publishes both Release Candidates and stable Releases. No longer blocked by #38, which is not planned.
