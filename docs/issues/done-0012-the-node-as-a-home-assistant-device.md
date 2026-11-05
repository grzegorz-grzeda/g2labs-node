---
number: 12
title: "The Node as a Home Assistant device"
status: closed
closed_reason: not_planned
labels: [ready-for-agent]
parent: 1
blocked_by: [8]
created: 2026-11-04
closed: 2026-11-04
---

# The Node as a Home Assistant device

## What to build

Each Node appears automatically in Home Assistant through MQTT discovery as a device with diagnostic sensors and Reboot and Publish status buttons, and its availability follows its online/offline status.

## Acceptance criteria

- [ ] On Broker connect the Node publishes HA discovery for one device keyed by its Node ID, using the friendly name when set
- [ ] Diagnostic sensors: Wi-Fi RSSI, current Known Network, uptime, firmware version, free memory, Sighting drop count
- [ ] Reboot and Publish status appear as HA buttons that send the existing commands
- [ ] Entity availability follows the retained online/offline status
- [ ] The discovery prefix is a setting, default `homeassistant`
- [ ] When `<prefix>/status` reports `online`, the Node re-announces
- [ ] Verified against a real Home Assistant: the device and entities appear and the buttons work
- [ ] Core tests on `native_sim` assert the discovery payloads and re-announcement

## Comments

### 2026-11-04 — grzegorz-grzeda

Dropped by ADR-0003 (docs/adr/0003-the-hub-owns-home-assistant-and-decoding.md): Nodes publish only raw Sightings and Status; decoding and everything Home Assistant sees move to the Hub, spec #17.
