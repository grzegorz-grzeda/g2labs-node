---
number: 8
title: "MQTT commands: Reboot and Publish status"
status: closed
closed_reason: completed
closed: 2026-11-04
labels: [ready-for-agent]
parent: 1
blocked_by: [6, 21]
assignee: claude
created: 2026-11-04
---

# MQTT commands: Reboot and Publish status

## What to build

A maintainer can reboot a Node or ask for fresh status by publishing a command to the Node's command topic.

## Acceptance criteria

- [x] A Reboot command restarts the Node
- [x] A Publish status command publishes status immediately
- [x] Unknown or malformed commands are ignored and logged, and never crash the Node
- [x] The command topic and payload format are documented alongside the other topics
- [x] Core tests on `native_sim` cover both commands and malformed input
- [x] The command payload has a schema and examples in `contract/`, and the topic is in the index; the contract checker passes

## Comments

### 2026-11-04 — claude

Done. The Node subscribes to `g2labs/<node-id>/command` on every connect. The commands are `{"command": "reboot"}` and `{"command": "publish_status"}`. Reboot publishes `offline`, disconnects, and reboots 1 s later. Unknown, malformed (including over 128 bytes) and retained commands are ignored and logged through a new `log_warning` port. The topic, schema and examples are in `contract/` and `docs/mqtt.md`.
