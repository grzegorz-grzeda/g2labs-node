---
number: 31
title: "CI check that the contract only grows"
status: open
labels: [ready-for-agent]
parent: 30
blocked_by: [21]
created: 2026-11-04
---

# CI check that the contract only grows

## What to build

ADR-0005 is enforced rather than remembered: a change that breaks the MQTT contract fails CI with a message saying what broke and to add a new topic instead, while changes that only add things pass.

## Acceptance criteria

- [ ] The contract checker compares the topic index and schemas on the current commit with those on `main` (on `main` itself, with the previous commit; on branches, with the merge base)
- [ ] It fails on: a removed topic, a removed property, a changed property type, a narrowed enum or set of allowed text values, a property that became required, and a changed retained flag or encoding
- [ ] It passes on: a new topic, a new optional property, a new enum value, new examples
- [ ] Each failure names the topic, the property and the kind of break, and says that a breaking change needs a new topic (ADR-0005)
- [ ] The checker has its own tests covering every failing and passing case above
- [ ] The firmware CI job runs the check
