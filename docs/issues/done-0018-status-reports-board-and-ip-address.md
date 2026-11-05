---
number: 18
title: "Status reports board and IP address"
status: closed
closed_reason: completed
closed: 2026-11-05
labels: [ready-for-agent]
parent: 1
blocked_by: [21]
assignee: claude
created: 2026-11-04
---

# Status reports board and IP address

## What to build

The Hub needs to know which image fits a Node and how to reach it if mDNS fails, so a Node's Status also reports its board and its IP address. Both are additive fields (ADR-0005).

## Acceptance criteria

- [x] Status carries `board`, the Zephyr board target the image was built for (e.g. `esp32c6_devkitc/esp32c6/hpcore`)
- [x] Status carries `ip`, the Node's current IPv4 address as a dotted string
- [x] `docs/mqtt.md` documents both fields
- [x] Core tests on `native_sim` assert both fields in the published Status
- [x] The Status schema and examples in `contract/` include `board` and `ip`; the contract checker passes

## Comments

### 2026-11-05 — claude

Done. Status now carries `board` (`CONFIG_BOARD_TARGET`, passed to the core in `struct node_info`) and `ip` (from a new `wifi_ipv4` port, formatted by the core). Both are optional in the Status schema, so an older Node's Status still validates (ADR-0005). `ip` is `null` when the Wi-Fi adapter has no address, the same as `rssi`. The Status buffer grew from 512 to 768 bytes, because a fully escaped name and SSID already overflowed it. Core tests on `native_sim` and the contract check pass, and both sysbuild builds compile. Not run on hardware.
