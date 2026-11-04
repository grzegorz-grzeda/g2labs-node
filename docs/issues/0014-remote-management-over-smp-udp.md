---
number: 14
title: "Remote management over SMP-UDP"
status: closed
closed_reason: completed
closed: 2026-11-04
labels: [ready-for-agent]
parent: 1
blocked_by: [4]
created: 2026-11-04
assignee: claude
---

# Remote management over SMP-UDP

## What to build

A maintainer can manage a Node over the network without a cable: upload images, run shell commands and read and change settings over SMP-UDP, and see which images a Node holds.

## Acceptance criteria

- [x] SMP over UDP is available once the Node has an IP address
- [x] Image upload over SMP-UDP works and the new image boots as a test image under MCUboot
- [x] Shell commands run over SMP-UDP
- [x] Settings can be read and changed over SMP-UDP; passwords are never returned
- [x] Images and their state (active, pending, confirmed) can be listed over SMP and the UART shell
- [x] Usage with a host tool (e.g. `mcumgr` or `smpmgr`) is documented
- [x] SMP-UDP has no authentication; this is documented as a known gap for the security work

## Comments

### 2026-11-04 — claude

Done: SMP over UDP port 1337 with the image, OS, shell and settings groups; settings through a `node` settings handler (staged writes, applied and saved on commit, password reads refused); `node image list` on the UART shell; usage and the missing authentication in `docs/management.md`, with an smpmgr settings plugin in `scripts/smpmgr/`. Built for both boards and the core tests pass; not yet tried on hardware, which the smoke test (#16) covers.
