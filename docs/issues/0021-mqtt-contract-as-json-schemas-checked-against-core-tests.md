---
number: 21
title: "MQTT contract as JSON Schemas, checked against core tests"
status: open
labels: [ready-for-agent]
parent: 1
blocked_by: []
created: 2026-11-04
---

# MQTT contract as JSON Schemas, checked against core tests

## What to build

The firmware and the Hub can't drift apart (ADR-0004): the MQTT contract lives in `contract/` as JSON Schemas with example payloads, and CI fails when the core publishes a payload that doesn't validate.

## Acceptance criteria

- [ ] `contract/` holds a JSON Schema for every payload the Node publishes today (availability, Status), plus example payloads in `contract/examples/`
- [ ] The core tests' fake ports dump every recorded publish, and a CI step validates each against the schema for its topic
- [ ] Every example validates against its schema in CI
- [ ] `docs/mqtt.md` links to the schemas and says the contract only grows (ADR-0005)
- [ ] Later tickets that add or change payloads (#8, #9, #10, Status fields) add or update their schemas
