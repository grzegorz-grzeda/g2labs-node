---
number: 16
title: "Hardware smoke test for ESP32-C6 and ESP32-S3"
status: open
labels: [ready-for-agent]
parent: 1
blocked_by: [9, 15, 19]
created: 2026-11-04
---

# Hardware smoke test for ESP32-C6 and ESP32-S3

## What to build

One opt-in test proves the thin adapters work on real hardware, identical for both boards: flash, provision through the shell, see the Node online in the Broker and on its mDNS name, receive Sightings, and update it over SMP.

## Acceptance criteria

- [ ] A pytest test under twister with a hardware map flashes the Node via sysbuild with MCUboot
- [ ] It provisions a Known Network and the Broker through the UART shell
- [ ] It verifies the retained `online` status, that `g2labs-<node-id>.local` resolves, and Sighting batches arriving
- [ ] It uploads a new image over SMP-UDP and verifies it boots and confirms
- [ ] The same test passes on ESP32-C6 and ESP32-S3
- [ ] How to run it (hardware map, Broker, Wi-Fi details) is documented; it is not part of default CI
