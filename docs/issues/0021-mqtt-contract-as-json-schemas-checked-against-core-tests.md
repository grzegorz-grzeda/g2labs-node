---
number: 21
title: "MQTT contract as JSON Schemas, checked against core tests"
status: open
labels: [ready-for-agent]
parent: 30
blocked_by: []
created: 2026-11-04
---

# MQTT contract as JSON Schemas, checked against core tests

## What to build

The firmware and the Hub can't drift apart (ADR-0004): the MQTT contract lives in `contract/` as JSON Schemas with example payloads, and CI fails when the core publishes a payload that doesn't validate.
## Acceptance criteria

- [ ] `contract/` holds the topic index, with publisher, retained flag and encoding per topic, and covers what a Node publishes today: availability (text: `online`/`offline`) and Status (JSON)
- [ ] The Status schema is JSON Schema 2020-12 and forbids additional properties; example payloads in `contract/examples/` include one without a friendly name and one with `rssi: null`
- [ ] The core tests' fake ports also write every recorded publish (test name, topic, payload, retained) to a file in the twister output
- [ ] A Python contract checker validates that file against the index and schemas: unknown topic, wrong retained flag, extra field or schema error fail with the test name, topic, payload and error
- [ ] The checker also validates every example against its schema and checks that `docs/mqtt.md` covers every topic in the index
- [ ] CI runs the checker after the core tests
- [ ] `docs/mqtt.md` says the contract is between Nodes and the Hub (ADR-0003), links to `contract/`, and states the contract only grows (ADR-0005); `CLAUDE.md` describes `contract/`
- [ ] The growth check against `main` is left to a follow-up ticket of #30
