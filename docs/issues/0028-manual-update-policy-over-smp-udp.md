---
number: 28
title: "Manual Update Policy over SMP-UDP"
status: open
labels: [ready-for-agent]
parent: 17
blocked_by: [23, 14, 15, 18, 19]
created: 2026-11-04
---

# Manual Update Policy over SMP-UDP

## What to build

The Manual Update Policy and the update engine the Follow Releases policy will reuse: upload a signed image to the Hub, pick a Node, and the Hub updates it over SMP-UDP and judges the outcome from the Node's own Status.

## Acceptance criteria

- [ ] A small SMP-UDP client in TypeScript (8-byte header + CBOR): image upload, image list, image test, reset
- [ ] The Hub resolves `g2labs-<node-id>.local` itself over mDNS and falls back to the `ip` in the Node's latest Status
- [ ] Uploading an image to the Hub requires choosing its board; the Hub reads the version from the MCUboot image header and stores the image
- [ ] Update on a Node: refuse if the image's board differs from the board in the Node's Status; otherwise upload, mark as test, reset
- [ ] Success means the Node comes back `online` with the new `fw_version` within a timeout; otherwise the update is failed (the Node reverts by itself, #15)
- [ ] Downgrades are allowed; Manual updates work on Nodes on Hold
- [ ] Progress is shown live; every update is kept in a history per Node
- [ ] Tests cover success, timeout, board mismatch and an unreachable Node, against a fake SMP endpoint and a fake Node publishing Status
