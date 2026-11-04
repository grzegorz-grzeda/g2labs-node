---
number: 11
title: "BTHome v2 decoding on Sightings"
status: closed
closed_reason: not_planned
labels: [ready-for-agent]
parent: 1
blocked_by: [9]
created: 2026-11-04
closed: 2026-11-04
---

# BTHome v2 decoding on Sightings

## What to build

Sightings from BTHome v2 devices also carry their decoded values, so simple consumers don't need a decoder. Decoding sits behind a per-format decoder interface so other formats can be added later. The raw payload is always kept.

## Acceptance criteria

- [ ] Unencrypted BTHome v2 advertisements decode temperature, humidity, battery and the other BTHome-defined measurements the decoder supports
- [ ] Decoded values are attached to the Sighting alongside the unchanged raw payload
- [ ] Encrypted or malformed BTHome payloads produce a Sighting with no decoded values and never crash the Node
- [ ] Decoders are selected through a per-format interface
- [ ] Core tests on `native_sim` cover decoding with real BTHome sample payloads, plus malformed and encrypted ones

## Comments

### 2026-11-04 — grzegorz-grzeda

Dropped by ADR-0003 (docs/adr/0003-the-hub-owns-home-assistant-and-decoding.md): Nodes publish only raw Sightings and Status; decoding and everything Home Assistant sees move to the Hub, spec #17.
