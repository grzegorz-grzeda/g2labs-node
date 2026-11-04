---
number: 6
title: "Broker connection and Node status"
status: closed
closed_reason: completed
labels: []
parent: 1
blocked_by: [4]
created: 2026-11-04
closed: 2026-11-04
---

# Broker connection and Node status

## What to build

A Node connects to the Broker and keeps everyone informed of its health: a retained online/offline status and a periodic status message under `g2labs/<node-id>/…`. This ticket fixes the Node's topic layout and JSON field names, which become the contract with the Aggregator.

## Acceptance criteria

- [x] Broker host, port, username and password are shell settings; the password is never shown
- [x] The Node ID is derived from the Wi-Fi MAC; an optional friendly name is a setting
- [x] The Node connects over plain TCP with username/password and reconnects automatically after any disconnect
- [x] On connect it publishes a retained `online` status; the last will publishes `offline`
- [x] Every 60 s it publishes status with firmware version, uptime, current Known Network and its RSSI, and free memory
- [x] Changing Broker settings takes effect without reflashing (reconnect or a clear 'reboot needed' message)
- [x] Topic names and JSON fields are documented in the repo
- [x] Core tests on `native_sim` assert the publishes (topic, payload, retained flag) and the last will

## Comments

### 2026-11-04 — grzegorz-grzeda

Done in 8daba8e (Broker connection and Node status), plus 0c9fe0f (resolve `.local` Broker hosts over mDNS, found while testing on hardware).

Checked on an ESP32-C6 (Node ID `404cca57c89c`) against the Broker at `g2labs-rpi.local`: retained `online` on `g2labs/404cca57c89c/availability`, and `status` every 60 s with all the fields in `docs/mqtt.md`. The last will (`offline`) is covered by the core tests but hasn't been seen on hardware yet. The 49 core tests pass on `native_sim`, and both boards build.
