---
number: 19
title: "mDNS name g2labs-<node-id>.local"
status: open
labels: [ready-for-agent]
assignee: claude
parent: 1
blocked_by: []
created: 2026-11-04
---

# mDNS name g2labs-<node-id>.local

## What to build

The Hub reaches each Node by a stable name rather than an IP address: every Node answers mDNS as `g2labs-<node-id>.local`.

## Acceptance criteria

- [x] The hostname is `g2labs-` followed by the Node ID (`CONFIG_NET_HOSTNAME="g2labs-"` with `CONFIG_NET_HOSTNAME_UNIQUE`), identical on both boards
- [ ] The mDNS responder answers for `<hostname>.local` once the Node has an IP address, and keeps answering after a move to another Known Network
- [x] The existing mDNS resolution of `.local` Broker hosts keeps working
- [x] The naming rule is documented in `docs/mqtt.md` next to the Node ID, as part of the contract
- [x] Verified on hardware: `avahi-resolve -n g2labs-<node-id>.local` returns the Node's IP address

## Comments

### 2026-11-05 — claude

Done in `prj.conf`: `CONFIG_NET_HOSTNAME="g2labs-"` with `CONFIG_NET_HOSTNAME_UNIQUE` and `CONFIG_MDNS_RESPONDER`, shared by both boards. Zephyr's unique postfix is the interface's link address in lowercase hex, the same Wi-Fi MAC `wifi_adapter_mac` reads for the Node ID. `node id` now also prints the mDNS name (from the Wi-Fi adapter, `wifi_adapter_mdns_name`), and boot logs an error if it doesn't carry the Node ID.

The responder answers with whatever IPv4 address the interface has at the time of the query, and the IPv4 stack rejoins the mDNS group when the interface comes up again, so a move to another Known Network should need no code in the app. Without probing (experimental in Zephyr) the Node doesn't announce its new address, so a cached old one lingers for up to the TTL; `docs/mqtt.md` says so.

Enabling the responder at first broke every Broker host name: the socket service thread then has 4 entries to poll, and at the default `CONFIG_ZVFS_POLL_MAX=3` it doesn't start, so the DNS resolver can't register. `CONFIG_ZVFS_POLL_MAX=4` fixes it, and `CONFIG_NET_MAX_CONN` goes to 7 for the responder's socket.

Verified on an ESP32-C6 (Node `404cca57c89c`): `avahi-resolve -n g2labs-404cca57c89c.local` returns its address, it connects to the Broker `g2labs-rpi.local`, the image confirmed itself, and SMP still answers. Not verified on hardware: an ESP32-S3 (it builds with the same configuration) or a move between two Known Networks (the board had only one), so the second criterion stays open until a Node with two Known Networks has been seen answering after a move.
