---
number: 48
title: "Show the Node's shell as VT100 in the monitor"
status: closed
closed_reason: completed
closed: 2026-11-19
labels: []
parent:
blocked_by: []
created: 2026-11-19
---

# Show the Node's shell as VT100 in the monitor

## What to build

`scripts/g2labs-node.py monitor` (and `flash`, which ends in it) shows the Zephyr shell's escape sequences as symbols (`␛[1;32m`), because miniterm's default filter replaces control characters. Pass them through to the terminal instead.

## Acceptance criteria

- [x] The monitor runs miniterm with `--filter direct`, so colours, the prompt and line editing render as VT100

## Comments

### 2026-11-19 — claude

Done.
