---
number: 21
title: "MQTT contract as JSON Schemas, checked against core tests"
status: closed
closed_reason: completed
closed: 2026-11-04
labels: [ready-for-agent]
parent: 30
blocked_by: []
created: 2026-11-04
---

# MQTT contract as JSON Schemas, checked against core tests

## What to build

The firmware and the Hub can't drift apart (ADR-0004): the MQTT contract lives in `contract/` as JSON Schemas with example payloads, and CI fails when the core publishes a payload that doesn't validate.
## Acceptance criteria

- [x] `contract/` holds the topic index, with publisher, retained flag and encoding per topic, and covers what a Node publishes today: availability (text: `online`/`offline`) and Status (JSON)
- [x] The Status schema is JSON Schema 2020-12 and forbids additional properties; example payloads in `contract/examples/` include one without a friendly name and one with `rssi: null`
- [x] The core tests' fake ports also write every recorded publish (test name, topic, payload, retained) to a file in the twister output
- [x] A Python contract checker validates that file against the index and schemas: unknown topic, wrong retained flag, extra field or schema error fail with the test name, topic, payload and error
- [x] The checker also validates every example against its schema and checks that `docs/mqtt.md` covers every topic in the index
- [x] CI runs the checker after the core tests
- [x] `docs/mqtt.md` says the contract is between Nodes and the Hub (ADR-0003), links to `contract/`, and states the contract only grows (ADR-0005); `CLAUDE.md` describes `contract/`
- [x] The growth check against `main` is left to a follow-up ticket of #30

## Comments

### 2026-11-04 — closed

Done. `contract/topics.json` indexes the Node's two topics (availability as text `online`/`offline`, retained; Status as JSON, not retained), with a strict JSON Schema 2020-12 for Status in `contract/schemas/` and examples in `contract/examples/` (including one without `name` and one with `rssi: null`). The fake ports dump every publish to `contract_publishes.jsonl` in the test's twister build directory, through a small host-side file (`tests/core/src/publish_dump_bottom.c`) because native_sim's libc can't write host files; the core is unchanged. `contract/check.py` validates the contract (schemas use core keywords only and forbid additional properties, examples match, `docs/mqtt.md` covers every topic) and, given the twister output, every recorded publish; failures name the test, topic, payload and error. Its own tests are in `contract/test_check.py`. CI runs both right after the core tests. The growth check against `main` is #31.
