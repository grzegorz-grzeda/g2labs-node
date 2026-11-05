---
number: 23
title: "Node registry and Status history in the Hub"
status: open
labels: [ready-for-agent]
assignee: claude
parent: 17
blocked_by: [22, 21]
created: 2026-11-04
---

# Node registry and Status history in the Hub

## What to build

The Hub knows the fleet: every Node seen on the Broker is registered automatically, its Status is kept as history, and the admin page shows it live.

## Acceptance criteria

- [ ] Subscribes to `g2labs/+/availability` and `g2labs/+/status`; a Node is registered the first time either arrives
- [ ] Every incoming payload is validated against `contract/`; invalid ones are logged and counted, never crash the Hub; unknown fields are ignored
- [ ] Hub TypeScript types for payloads are generated from the schemas
- [ ] Status history is stored and pruned after a configurable retention (default 30 days)
- [ ] Node list page: availability, friendly name, firmware version, board, Known Network, RSSI, last seen, updated live
- [ ] Node detail page: Status history charts (RSSI, free memory, uptime resets)
- [ ] Forget deletes a Node's history and clears its retained topics on the Broker
- [ ] Tests feed the examples from `contract/examples/` and assert on API responses
