---
number: 18
title: "Status reports board and IP address"
status: open
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

- [ ] Status carries `board`, the Zephyr board target the image was built for (e.g. `esp32c6_devkitc/esp32c6/hpcore`)
- [ ] Status carries `ip`, the Node's current IPv4 address as a dotted string
- [ ] `docs/mqtt.md` documents both fields
- [ ] Core tests on `native_sim` assert both fields in the published Status
- [ ] The Status schema and examples in `contract/` include `board` and `ip`; the contract checker passes
