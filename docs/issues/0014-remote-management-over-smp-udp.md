---
number: 14
title: "Remote management over SMP-UDP"
status: open
labels: [ready-for-agent]
parent: 1
blocked_by: [4]
created: 2026-11-04
---

# Remote management over SMP-UDP

## What to build

A maintainer can manage a Node over the network without a cable: upload images, run shell commands and read and change settings over SMP-UDP, and see which images a Node holds.

## Acceptance criteria

- [ ] SMP over UDP is available once the Node has an IP address
- [ ] Image upload over SMP-UDP works and the new image boots as a test image under MCUboot
- [ ] Shell commands run over SMP-UDP
- [ ] Settings can be read and changed over SMP-UDP; passwords are never returned
- [ ] Images and their state (active, pending, confirmed) can be listed over SMP and the UART shell
- [ ] Usage with a host tool (e.g. `mcumgr` or `smpmgr`) is documented
- [ ] SMP-UDP has no authentication; this is documented as a known gap for the security work
