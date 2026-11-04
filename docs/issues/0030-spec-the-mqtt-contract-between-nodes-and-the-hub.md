---
number: 30
title: "Spec: the MQTT contract between Nodes and the Hub"
status: open
labels: [ready-for-agent]
parent:
blocked_by: []
created: 2026-11-04
---

# Spec: the MQTT contract between Nodes and the Hub

## Problem Statement

Nodes (#1) and the Hub (#17) talk only through the Broker, and they are built in different languages, released at different times, and run in mixed versions across the fleet. Today the only description of what a Node publishes is prose in `docs/mqtt.md`, which still calls it a contract "with the Aggregator and Home Assistant". Nothing checks that the firmware publishes what the prose says, or that the Hub accepts it. ADR-0004 (one repo with a shared contract) and ADR-0005 (the contract only grows) have been decided, but no spec says what the contract consists of, who checks it, or how it grows. The pieces are scattered across tickets: the firmware-side check (#21), Hub topics (#22, #27) and the release manifest (#20).

## Solution

One machine-readable contract that both sides are checked against in CI:

- Every topic a Node or the Hub publishes is listed in one index, with who publishes it, whether it is retained, and how its payload is encoded, either as plain text with a fixed set of values or as JSON with a JSON Schema.
- Each payload has example messages that both sides use in their tests.
- The firmware's core tests prove that every payload the Node publishes matches the contract. The Hub's tests prove that it accepts every example.
- CI rejects any change that breaks ADR-0005: a removed field, a changed type, a removed topic.
- `docs/mqtt.md` stays the human explanation and links to the machine-readable contract.

## User Stories

### The contract

1. As a developer, I want every topic a Node or the Hub publishes listed in one index, with publisher, retained flag and payload encoding, so that there is one place to look up what's on the Broker.
2. As a developer, I want each JSON payload described by a JSON Schema, so that both sides can check messages by machine instead of reading prose.
3. As a developer, I want plain-text payloads (like `online`/`offline`) described by their allowed values, so that they're covered by the same checks as JSON ones.
4. As a developer, I want example messages for every payload, including edge cases such as a Status with no friendly name and `rssi: null`, so that tests on both sides start from the same realistic data.
5. As a developer, I want the contract to cover the Node's topics (availability, Status, commands, Sighting batches), so that the Hub's input is fully described.
6. As a developer, I want the contract to cover the Hub's own topics (`g2labs/hub/availability`, `g2labs/sensor/<sensor-id>/state`), so that other consumers can rely on them too.
7. As a developer, I want the release manifest (#20) described by a schema in the same place, so that the release workflow and the Hub's Follow Releases Update Policy agree on it.
8. As a developer, I want the Node's mDNS naming rule (`g2labs-<node-id>.local`, #19) recorded with the contract, so that the Hub's way of reaching Nodes is written down as a promise.

### Checking the firmware

9. As a developer, I want every payload the core publishes in its tests checked against the contract, so that the firmware can't publish anything undocumented or malformed.
10. As a developer, I want a firmware payload with a field the schema doesn't list to fail the check, so that every field a Node sends is documented.
11. As a developer, I want a publish to a topic that isn't in the index to fail the check, so that new topics can't appear without being described.
12. As a developer, I want the retained flag of each recorded publish checked against the index, so that retained-ness is part of the contract.
13. As a developer, I want a failing check to name the test, topic, payload and the schema error, so that I can fix it without digging.

### Checking the Hub

14. As a developer, I want the Hub's tests to feed it every example in the contract and expect it to accept them, so that the Hub handles everything the contract allows.
15. As a developer, I want the Hub to accept payloads with fields it doesn't know, so that a Node running newer firmware than the Hub expects still works (ADR-0005).
16. As a developer, I want the Hub's TypeScript types for payloads generated from the schemas, so that the Hub's code can't drift from the contract.
17. As a developer, I want every payload the Hub publishes in its tests checked against the contract the same way as the firmware's, so that the Hub's own topics are held to the contract too.

### Growing the contract

18. As a developer, I want CI to fail when a change removes a topic, removes a field, changes a field's type, or makes an optional field required, compared with `main`, so that ADR-0005 is enforced rather than remembered.
19. As a developer, I want adding a topic or an optional field to pass that check, so that the contract can grow freely.
20. As a developer, I want the check to tell me to add a new topic when I try a breaking change, so that the fix is obvious.

### Self-consistency and docs

21. As a developer, I want CI to check that every example matches its schema and that every topic in the index is described in `docs/mqtt.md`, so that the human docs and the contract can't drift apart.
22. As a developer, I want `docs/mqtt.md` to say the contract is between Nodes and the Hub, list the Hub's topics next to the Node's, and link to the schemas, so that it reflects ADR-0003.
23. As a developer, I want `CLAUDE.md` and `README.md` to describe the repo's three parts (firmware, `service/`, `contract/`) and the contract rules, so that agents and humans find the contract before changing a payload.

## Implementation Decisions

- **Location and ownership (ADR-0004).** The contract lives in the repo's `contract/` directory and belongs to neither the firmware nor the Hub. A change to it lands in the same commit as the firmware and Hub changes that need it.
- **Topic index.** One index file maps topic patterns (with `<node-id>` and `<sensor-id>` placeholders) to: publisher (Node or Hub), retained flag, encoding (`text` with allowed values, or `json` with a schema), and a one-line meaning. Both sides' checks start from it.
- **Schemas.** JSON Schema draft 2020-12, one per JSON payload, using only core keywords (type, properties, required, enum, minimum/maximum, pattern) so that the Python validator in firmware CI and `ajv` in the Hub agree.
- **Strict schemas, lenient consumers.** Schemas forbid additional properties, so the producer checks catch undocumented fields (story 10). The Hub validates incoming messages with additional properties allowed, so newer Nodes don't break it (story 15, ADR-0005). The Hub's runtime validation logs and counts invalid messages and never crashes.
- **No version field.** Payloads carry no schema version (ADR-0005). Breaking changes go to a new topic.
- **Firmware producer check.** The core tests' fake ports already record every publish. They additionally write each one (test name, topic, payload, retained flag) to a file in the twister output. After the core tests, a CI step runs the contract checker over that file. No change to the core and no new seam inside it.
- **Hub producer and consumer checks.** The Hub's tests go through its MQTT boundary from #17: examples in, recorded publishes out, both checked with the same index and schemas.
- **Growth check.** A script compares the index and schemas on the current commit with those on `main` (or the merge base for branches) and fails on removed topics, removed properties, changed types, narrowed enums, or new required properties. Adding topics, optional properties or enum values passes.
- **One checker, two CI jobs.** The contract checker (example self-check, docs coverage, producer-dump validation, growth check) is a Python script, because firmware CI already has Python. The firmware job and the Hub job both run it.
- **Release manifest.** Its schema lives in `contract/` like the MQTT schemas, but it is not in the topic index because it isn't published on the Broker.
- **Out of the contract.** Home Assistant discovery payloads published by the Hub follow HA's format, which HA owns, so they get no schemas of ours. Their topics are listed in the index only as publisher Hub, encoding "HA discovery".

## Testing Decisions

- **What a good test is.** Checks look only at what crosses the Broker: topic, payload, retained flag. Nothing inspects core or Hub internals. The contract is the shared expectation, and each side is tested at its outer boundary.
- **Seam 1, firmware: the existing core seam on `native_sim`.** The ztest suite in `tests/core/` already drives the core through fake ports and asserts on recorded publishes. The fake ports gain a dump of what they record, and the contract checker validates the dump. Prior art: `tests/core/src/fake_ports.c`, and the `test_broker.c` assertions on Status payloads.
- **Seam 2, Hub: its MQTT boundary from #17.** Hub tests publish every example in, assert the Hub accepts them (and that a payload with an extra field is still accepted), and validate everything the Hub publishes.
- **Seam 3, the contract itself.** The checker validates examples against schemas, checks that `docs/mqtt.md` covers the index, and runs the growth check against `main`. The checker has its own small tests: a removed field, a type change and a new required field must each fail, and a new optional field must pass.
- **CI.** The firmware job runs seam 1 and seam 3, and the Hub job runs seam 2 and seam 3, on every push.

## Out of Scope

- Schemas for Home Assistant discovery payloads (HA's format).
- Binary encodings (CBOR) for any topic; when they come, they will be new topics under ADR-0005.
- Generating C code from the schemas for the firmware.
- AsyncAPI or other full API description formats.
- Publishing the contract as a separate package or repo (revisit with ADR-0004 if the repo is ever split).
- Runtime validation on the Node.

## Further Notes

- Tickets that already depend on this: #21 (the firmware-side check, now the first ticket of this spec), #8 (command payload), #9 (Sighting batch payload), #10 (timestamps), #18 (`board` and `ip` in Status), #19 (mDNS naming rule), #20 (release manifest), #22 (`g2labs/hub/availability`), #23 (Hub runtime validation and generated types), #27 (`g2labs/sensor/<sensor-id>/state`). Each adds or extends its part of the contract.
- Decisions: ADR-0003 (the Hub owns HA), ADR-0004 (one repo with a shared contract), ADR-0005 (the contract only grows).
