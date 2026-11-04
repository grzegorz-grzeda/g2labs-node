---
number: 7
title: "Fail over when the Broker is unreachable"
status: closed
closed_reason: completed
labels: [ready-for-agent]
parent: 1
blocked_by: [5, 6]
created: 2026-11-04
closed: 2026-11-04
---

# Fail over when the Broker is unreachable

## What to build

A Node on a Known Network with no upstream connection doesn't get stranded: after the Broker has been unreachable for a sustained period, it moves to the strongest other Known Network.

## Acceptance criteria

- [ ] After the Broker is unreachable for the configured period (several minutes by default), the Node rescans and joins the strongest other Known Network
- [ ] Shorter Broker outages only trigger reconnects on the same network
- [ ] With only one Known Network, it stays on it and keeps retrying
- [ ] Core tests on `native_sim` cover the timeout, the move, and the single-network case
