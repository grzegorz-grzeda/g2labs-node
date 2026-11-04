---
number: 10
title: "SNTP-synced Sighting timestamps"
status: open
labels: [ready-for-agent]
parent: 1
blocked_by: [9]
created: 2026-11-04
---

# SNTP-synced Sighting timestamps

## What to build

Sightings from different Nodes can be compared in time: once a Node syncs with SNTP, every Sighting carries epoch milliseconds.

## Acceptance criteria

- [ ] The Node syncs time with SNTP after getting an IP address and resyncs periodically
- [ ] After sync, Sightings carry an epoch-millisecond timestamp and no 'not synced' flag
- [ ] Before sync, Sightings keep uptime plus the 'not synced' flag and are not held back
- [ ] Core tests on `native_sim` cover the switch from unsynced to synced
