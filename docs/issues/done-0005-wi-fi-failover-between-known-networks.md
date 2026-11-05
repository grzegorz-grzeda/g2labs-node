---
number: 5
title: "Wi-Fi failover between Known Networks"
status: closed
closed_reason: completed
labels: [ready-for-agent]
parent: 1
blocked_by: [4]
created: 2026-11-04
closed: 2026-11-04
---

# Wi-Fi failover between Known Networks

## What to build

A Node recovers from Wi-Fi trouble by itself: it rides out brief glitches on its current Known Network, moves to the strongest other Known Network when the current one is gone, and keeps rescanning when none is reachable. It never roams away from a healthy network.

## Acceptance criteria

- [ ] On a link drop the Node retries its current Known Network first
- [ ] After a reconnect timeout or no IP address within the DHCP timeout, it rescans and joins the strongest other Known Network
- [ ] With no Known Network reachable, it rescans periodically and joins one when it returns
- [ ] While healthy it stays put even when a stronger Known Network appears
- [ ] Timeouts are named constants or settings
- [ ] Core tests on `native_sim` cover each of these scenarios
