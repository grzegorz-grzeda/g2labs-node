---
number: 26
title: "BTHome decoding and the Sensor lifecycle"
status: open
labels: [ready-for-agent]
parent: 17
blocked_by: [25]
created: 2026-11-04
---

# BTHome decoding and the Sensor lifecycle

## What to build

The Hub decodes BTHome v2 from raw Sightings and lets the user turn the Advertisers it understands into Sensors with a lifecycle: Discovered, Adopted, Unavailable, Removed.

## Acceptance criteria

- [ ] Decoders are pluggable modules behind one interface; BTHome v2 (unencrypted) is the first, decoding temperature, humidity, battery and the other measurements it supports
- [ ] The BTHome packet ID is used to deduplicate repeated advertisements
- [ ] Malformed or encrypted payloads are ignored without crashing the Hub
- [ ] An Advertiser a decoder understands is shown as Discovered, with live decoded values
- [ ] Adopt turns it into a Sensor with a name; Sensors and their last values are stored in SQLite
- [ ] An optional setting adopts decodable Advertisers automatically (off by default)
- [ ] A Sensor with no reception for a configurable time (default 15 min) is Unavailable, and becomes available again on the next reception
- [ ] Remove deletes a Sensor and its history
- [ ] Tests use real BTHome sample payloads, plus malformed and encrypted ones, and walk a Sensor through every state
