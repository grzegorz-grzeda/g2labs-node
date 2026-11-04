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

- [ ] A workflow on `v*` tags builds both boards through sysbuild, as CI does
- [ ] The release gets `g2labs-node-<version>-<board>.signed.bin` per board (board target with `/` replaced by `-`), the MCUboot-signed Node image suitable for SMP upload
- [ ] The release gets `manifest.json`: version plus, per board target, the asset name and its SHA-256
- [ ] The manifest has a JSON Schema in `contract/`
- [ ] The tag must match `VERSION`, or the workflow fails
- [ ] Tags with a pre-release suffix (e.g. `v0.3.0-rc1`) produce GitHub pre-releases
