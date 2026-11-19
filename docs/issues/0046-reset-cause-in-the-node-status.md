---
number: 46
title: "Report the cause of the last reset in the Node's Status"
status: open
labels: [needs-triage]
parent: 1
blocked_by: [45]
created: 2026-11-19
---

# Report the cause of the last reset in the Node's Status

## What to build

`node status` (#45) shows why the Node last reset (watchdog, power-on, software, …), but only on the shell. The Hub would see watchdog resets across the fleet if the Status carried it too: an optional `reset_cause` field in the contract (ADR-0005: it only grows), `docs/mqtt.md`, the core's Status publish and the Hub's Node registry.

## Acceptance criteria

- [ ] The Status schema has an optional `reset_cause`, with its values listed, and an example
- [ ] The Node reports it; the core tests' publishes pass the contract check
- [ ] The Hub stores and shows it
