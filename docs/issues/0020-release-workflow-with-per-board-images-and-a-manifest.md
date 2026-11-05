---
number: 20
title: "Release workflow with per-board images and a manifest"
status: open
labels: [ready-for-agent]
parent: 1
blocked_by: [38]
created: 2026-11-04
---

# Release workflow with per-board images and a manifest

## What to build

The Hub's Follow Releases Update Policy (#17) needs releases it can use: pushing a `v*` tag produces a GitHub Release with one signed image per board and a manifest that tells the Hub which image is for which board.

## Acceptance criteria

- [ ] A Release Candidate is published when a `v*-rcN` tag is pushed; a stable Release is published when a release PR (`release/vX.Y.Z`, setting `VERSION` without `-dev`) is merged into `main`, the workflow creating the tag itself (see `docs/agents/development-flow.md`)
- [ ] Both build both boards through sysbuild, as CI does
- [ ] The release gets `g2labs-node-<version>-<board>.signed.bin` per board (board target with `/` replaced by `-`), the MCUboot-signed Node image suitable for SMP upload
- [ ] The release gets `manifest.json`: version plus, per board target, the asset name and its SHA-256
- [ ] The manifest has a JSON Schema in `contract/`
- [ ] The tag must match `VERSION`, or the workflow fails
- [ ] Release Candidates (e.g. `v0.3.0-rc1`) produce GitHub pre-releases
- [ ] The tag created on a release PR merge must also trigger the Hub image publish (#22); a tag pushed with the default `GITHUB_TOKEN` triggers no workflows, so publish from the same run or use a token that does

## Comments

### 2026-11-04 — claude

Reworked for the development flow (ADR-0006): stable Releases come from the maintainer merging a release PR, not from a pushed tag; Release Candidates still come from pushed tags.
