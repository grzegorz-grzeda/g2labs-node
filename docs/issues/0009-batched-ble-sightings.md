---
number: 9
title: "Batched BLE Sightings"
status: open
labels: [ready-for-agent]
parent: 1
blocked_by: [6, 21]
created: 2026-11-04
---

# Batched BLE Sightings

## What to build

A Node passively scans BLE and reports every advertisement it hears as a Sighting, from any device and unfiltered (ADR-0001), in one batched message per time window. A bounded buffer keeps memory fixed and counts what it had to drop.

## Acceptance criteria

- [ ] Scanning is passive and continuous, with the duty cycle as a setting (default around 50%)
- [ ] Every advertisement becomes a Sighting with address, address type, RSSI and raw payload; nothing is filtered or deduplicated
- [ ] Sightings carry uptime and a 'not synced' flag
- [ ] Sightings are published as one batch per window (about 1 s) per Node, never as HA entities
- [ ] On buffer overflow the oldest Sightings are dropped and a drop count is included in the next batch and in the status message
- [ ] While the Broker is unreachable the buffer stays bounded and drops are counted
- [ ] Batch topic and JSON fields are documented
- [ ] Core tests on `native_sim` cover batching, overflow and the drop count
- [ ] The Sighting batch payload has a schema and examples in `contract/`, and the topic is in the index; the contract checker passes
