---
number: 8
title: "MQTT commands: Reboot and Publish status"
status: open
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

- [ ] A Reboot command restarts the Node
- [ ] A Publish status command publishes status immediately
- [ ] Unknown or malformed commands are ignored and logged, and never crash the Node
- [ ] The command topic and payload format are documented alongside the other topics
- [ ] Core tests on `native_sim` cover both commands and malformed input
- [ ] The command payload has a schema and examples in `contract/`, and the topic is in the index; the contract checker passes
