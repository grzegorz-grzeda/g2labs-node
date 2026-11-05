---
number: 2
title: "Walking skeleton: bootable firmware, UART shell and core test harness"
status: closed
closed_reason: completed
labels: [ready-for-agent]
parent: 1
blocked_by: []
created: 2026-11-04
closed: 2026-11-04
---

# Walking skeleton: bootable firmware, UART shell and core test harness

## What to build

A Node firmware that builds through sysbuild with MCUboot (development key) for `esp32c6_devkitc/esp32c6/hpcore` and `esp32s3_devkitc/esp32s3/procpu`, boots, and offers a UART shell with a command that prints the firmware version. The Node core exists behind its adapter boundary (inputs: platform events and settings; outputs: MQTT publishes, Wi-Fi requests, image confirmation), with fake adapters that let a ztest drive the core on `native_sim` under twister. This is the skeleton every later ticket grows.

## Acceptance criteria

- [ ] Both board targets build with MCUboot via sysbuild from a clean checkout (after `west blobs fetch hal_espressif`)
- [ ] Flashed to an ESP32-C6, the Node boots under MCUboot and the UART shell prints the firmware version
- [ ] The Node core has no direct dependency on Zephyr Wi-Fi, Bluetooth, MQTT or MCUboot APIs; those are reached only through adapters
- [ ] A fake-adapter ztest on `native_sim` passes under twister with one command
- [ ] CLAUDE.md's build and test commands are updated to the real ones

## Comments

### 2026-11-04 — grzegorz-grzeda

Implemented in 1c9f2ad: sysbuild + MCUboot (dev key) builds for esp32c6_devkitc/esp32c6/hpcore and esp32s3_devkitc/esp32s3/procpu; verified booting under MCUboot on an ESP32-C6 with `node version` printing `0.1.0-dev+0` on the UART shell; Node core in `core/` behind `struct node_ports` with fake-adapter ztests passing via `west twister -T tests -p native_sim`; CLAUDE.md updated. The ESP32-S3 is build-verified only (hardware run is covered by #16). Core event/settings inputs are deliberately left to #4 and #6.
