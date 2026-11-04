---
number: 32
title: "Check the Hub's publishes against the contract"
status: open
labels: [ready-for-agent]
parent: 30
blocked_by: [21, 22]
created: 2026-11-04
---

# Check the Hub's publishes against the contract

## What to build

The Hub is held to the MQTT contract the same way the firmware is: everything the Hub publishes in its tests is checked against the topic index and schemas, and the Hub's CI job runs the whole contract checker.

## Acceptance criteria

- [ ] The Hub's tests record every publish at its MQTT boundary (test name, topic, payload, retained) to a file, in the same format the firmware's fake ports use
- [ ] The contract checker validates that file: unknown topic, wrong retained flag, extra field or schema error fail with the test name, topic, payload and error
- [ ] HA discovery publishes are matched against their index entries by topic only, not validated against a schema
- [ ] The Hub CI job runs the checker: the example self-check, the docs check, the Hub publish check, and the growth check once #31 exists
- [ ] The README describes the repo's three parts (firmware, `service/`, `contract/`), how to build and test each, and that payload changes start in `contract/`
