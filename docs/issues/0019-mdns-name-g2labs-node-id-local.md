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

- [ ] The hostname is `g2labs-` followed by the Node ID (`CONFIG_NET_HOSTNAME="g2labs-"` with `CONFIG_NET_HOSTNAME_UNIQUE`), identical on both boards
- [ ] The mDNS responder answers for `<hostname>.local` once the Node has an IP address, and keeps answering after a move to another Known Network
- [ ] The existing mDNS resolution of `.local` Broker hosts keeps working
- [ ] The naming rule is documented in `docs/mqtt.md` next to the Node ID, as part of the contract
- [ ] Verified on hardware: `avahi-resolve -n g2labs-<node-id>.local` returns the Node's IP address
