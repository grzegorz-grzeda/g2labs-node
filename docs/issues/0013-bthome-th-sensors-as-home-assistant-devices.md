---
number: 13
title: "BTHome TH sensors as Home Assistant devices"
status: closed
closed_reason: not_planned
labels: [ready-for-agent]
parent: 1
blocked_by: [11, 12]
created: 2026-11-04
closed: 2026-11-04
---

# BTHome TH sensors as Home Assistant devices

## What to build

Every BTHome TH sensor heard by any Node appears in Home Assistant as a single device, announced identically by every Node under an ID derived from the sensor's MAC (ADR-0002), with values updated by whichever Node heard it most recently.

## Acceptance criteria

- [ ] A Node announces each BTHome TH sensor once per MQTT session, with device and entity IDs derived from the sensor MAC
- [ ] Entities: temperature, humidity, and battery when present
- [ ] State goes to a topic keyed by the sensor, not the Node
- [ ] Re-announces all known TH sensors when `<prefix>/status` reports `online`
- [ ] A setting turns TH-sensor announcing off; Sightings are unaffected
- [ ] Verified with two Nodes hearing one sensor: HA shows one device
- [ ] Core tests on `native_sim` cover once-per-session announcing, identical payloads from different Node IDs, re-announcement and the off setting

## Comments

### 2026-11-04 — grzegorz-grzeda

Dropped by ADR-0003 (docs/adr/0003-the-hub-owns-home-assistant-and-decoding.md): Nodes publish only raw Sightings and Status; decoding and everything Home Assistant sees move to the Hub, spec #17.
