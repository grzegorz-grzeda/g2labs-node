---
number: 27
title: "Sensors as Home Assistant devices"
status: open
labels: [ready-for-agent]
parent: 17
blocked_by: [26, 24]
created: 2026-11-04
---

# Sensors as Home Assistant devices

## What to build

Every adopted Sensor appears in Home Assistant as a device, announced only by the Hub, with stable IDs derived from the sensor's MAC and state published only when it changes.

## Acceptance criteria

- [ ] Adopting a Sensor publishes HA discovery for one device with its decoded entities (e.g. temperature, humidity, battery), with device and entity IDs derived from the sensor's MAC
- [ ] State goes to `g2labs/sensor/<sensor-id>/state`, documented in `docs/mqtt.md` and `contract/`
- [ ] State is published when a value changes, and otherwise at least every few minutes, never on every advertisement
- [ ] A "heard by" attribute shows the Node with the strongest reception and its RSSI
- [ ] Entity availability follows both `g2labs/hub/availability` and the Sensor's Unavailable state
- [ ] Removing a Sensor clears its retained discovery, so HA deletes the device
- [ ] Sensors are announced again when `<prefix>/status` reports `online`
- [ ] Verified against a real Home Assistant with two Nodes hearing one sensor: HA shows one device
