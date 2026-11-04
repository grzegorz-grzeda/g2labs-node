---
number: 4
title: "Join the strongest Known Network"
status: closed
closed_reason: completed
labels: [ready-for-agent]
parent: 1
blocked_by: [2]
created: 2026-11-04
closed: 2026-11-04
---

# Join the strongest Known Network

## What to build

An installer adds Known Networks through the UART shell, and at boot the Node joins the strongest reachable one. Known Networks persist across reboots and image updates.

## Acceptance criteria

- [ ] Shell commands add, list and remove Known Networks; listing never shows passwords
- [ ] Adding a 6th Known Network is rejected with a clear message
- [ ] Known Networks persist across reboots
- [ ] At boot the Node scans and joins the strongest reachable Known Network
- [ ] Adding a Known Network while offline takes effect without a reboot
- [ ] The shell shows the current Known Network, its RSSI and the IP address
- [ ] Core tests on `native_sim` cover strongest-network selection, the 5-network cap and no reachable network

## Comments

### 2026-11-04 — grzegorz-grzeda

Implemented in b23aea0, with a CI fix in 3d71153 (hostap module for the ESP32 Wi-Fi driver's legacy mbedtls sources). Verified on an ESP32-C6: `node network add|remove|list|status` work with passwords never shown, a 6th Known Network is rejected, Known Networks persist across reboots, the Node joins the strongest reachable Known Network at boot and right after an add while offline, and status shows the network, RSSI and IP address. 19 core tests on native_sim (strongest-network selection, the 5-network cap, no reachable network, plus removal during a scan, out-of-state events and refused requests) pass in CI: https://github.com/grzegorz-grzeda/g2labs-node/actions/runs/41757379262. Known gap for #5: a join that never completes (e.g. no DHCP answer) waits indefinitely until #5's connect/DHCP timeouts land.
