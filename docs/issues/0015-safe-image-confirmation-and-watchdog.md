---
number: 15
title: "Safe image confirmation and watchdog"
status: open
labels: [ready-for-agent]
parent: 1
blocked_by: [6, 14]
created: 2026-11-04
---

# Safe image confirmation and watchdog

## What to build

An update can't strand a Node: a new image confirms itself only after its first successful Broker connection, and a hardware watchdog reboots a hung image so MCUboot reverts it.

## Acceptance criteria

- [ ] A pending test image is confirmed only after its first successful Broker connection, never before
- [ ] An image that never reaches the Broker reverts to the previous one on the next reboot
- [ ] A hardware watchdog is fed while the Node is healthy; a hung image reboots and, if unconfirmed, reverts
- [ ] Verified on hardware: an image with a broken Broker setting reverts and the Node is reachable over SMP again
- [ ] Core tests on `native_sim` cover confirm-after-Broker and no confirm before
