---
number: 45
title: "Show the Node's whole state with one shell command"
status: closed
closed_reason: completed
closed: 2026-11-19
labels: [ready-for-agent]
parent: 1
blocked_by: []
created: 2026-11-19
assignee: claude
---

# Show the Node's whole state with one shell command

## What to build

Diagnosing a Node over its serial shell (or SMP's shell group) takes several commands today, and some of the state isn't shown at all. Add `node status`: one summary of what the Node knows about itself, a few `key: value` lines, so a glance tells whether it is healthy and where it is stuck.

- Firmware version and board; Node ID, friendly name, mDNS name.
- Uptime and the cause of the last reset (watchdog, power-on, software, …).
- Free heap.
- Wi-Fi: what the Node is doing (offline, scanning, joining a Known Network, waiting for an IP address, retrying, on it), the Known Network, RSSI, IPv4 address and MAC.
- Broker: host and port, and its connection: connected and for how long, or unreachable for how long and when the Node moves to another Known Network.
- The running image's version and whether it is confirmed.

The core gets one query, `node_core_state()`, a snapshot of its Wi-Fi and Broker state, tested through the fake ports like any other public API. The shell's existing commands stay.

Also enable Zephyr's `net` and `kernel` shells (`net iface`, `net ping`, `kernel threads`, `kernel stacks`, `kernel reboot`, …) for deeper digging. Not the `wifi` shell: its `wifi connect` bypasses the core and fights the Known Network failover.

`scripts/g2labs-node.py` gets a `status` subcommand mirroring `node status`, and `show` starts with it.

Out of scope: reporting the reset cause in the MQTT Status (#46).

## Acceptance criteria

- [x] `node_core_state()` reports the Wi-Fi phase and the Known Network being joined or joined, the Broker state, how long the Broker has been connected or unreachable, how long until failover (only with another Known Network to move to), and whether the image is confirmed; core tests cover each
- [x] `node status` prints all the fields above
- [x] The `net` and `kernel` shells are on, the `wifi` shell is not; both boards build
- [x] `scripts/g2labs-node.py status` exists and `show` starts with it
- [x] CLAUDE.md lists `node status`

## Comments

### 2026-11-19 — claude

Done. `node status` prints version and board, Node ID, name, mDNS name, uptime and last reset cause (hwinfo), free heap, Wi-Fi phase with RSSI, IP and MAC, the Broker's connection with how long it has been up or unreachable and when the Node moves on, and the running image. The core's `node_core_state()` is covered by `tests/core/src/test_state.c`. Tested: core tests and the contract check, both sysbuild builds; not yet on hardware.
