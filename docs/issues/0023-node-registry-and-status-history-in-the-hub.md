---
number: 23
title: "Node registry and Status history in the Hub"
status: closed
closed_reason: completed
closed: 2026-11-05
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

- [x] Subscribes to `g2labs/+/availability` and `g2labs/+/status`; a Node is registered the first time either arrives
- [x] Every incoming payload is validated against `contract/`; invalid ones are logged and counted, never crash the Hub; unknown fields are ignored
- [x] Hub TypeScript types for payloads are generated from the schemas
- [x] Status history is stored and pruned after a configurable retention (default 30 days)
- [x] Node list page: availability, friendly name, firmware version, board, Known Network, RSSI, last seen, updated live
- [x] Node detail page: Status history charts (RSSI, free memory, uptime resets)
- [x] Forget deletes a Node's history and clears its retained topics on the Broker
- [x] Tests feed the examples from `contract/examples/` and assert on API responses

## Comments

### 2026-11-05 — claude

Done on `feat/23-node-registry`. The Hub subscribes to `g2labs/+/availability` and `g2labs/+/status` and validates every payload with types and schemas generated from `contract/` into `service/src/contract.generated.ts` (`npm run contract`; a test fails when it is stale). Status history is pruned after `HUB_STATUS_RETENTION_DAYS` (default 30). The admin page lists the Nodes and updates live over SSE (`/api/events`), and each Node's page has RSSI, free memory and uptime charts with uptime resets marked. Forget deletes the Node with its history and clears its retained availability. A Node that is still running comes back, online, with its next Status. Tests feed every example in `contract/examples/` through MQTT and assert on `/api` responses.

