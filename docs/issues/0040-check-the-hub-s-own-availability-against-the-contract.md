---
number: 40
title: "Check the Hub's own availability against the contract"
status: closed
closed_reason: completed
closed: 2026-11-05
labels: [ready-for-agent]
assignee: claude
parent: 17
blocked_by: [39]
created: 2026-11-05
---

# Check the Hub's own availability against the contract

## What to build

A follow-up to #39, from the review of its PR.

- `service/src/broker.ts` hard-codes the Hub's availability topic (`g2labs/hub/availability`) and its `online` / `offline` payloads. The generated `HubAvailability` type exists but nothing uses it.
- `service/src/nodes.ts` still writes `'online'` by hand in two places.
- `service/scripts/gen-contract.ts` names each text topic's type from its publisher and last topic level. Two topics with the same publisher and last level would both get the same name, and the clash would only show up as a compile error in the generated file.

Behaviour stays the same.

## Acceptance criteria

- [x] The Hub's availability topic and payloads are checked against the generated contract types, so a value not in the contract fails the typecheck
- [x] So are the `online` literals in `nodes.ts`
- [x] The generator refuses a contract in which two topics would get the same type name, naming both topics
- [x] The existing Hub tests pass unchanged

## Comments

### 2026-11-05 — claude

Done. `broker.ts` checks the Hub's availability topic against the contract's Hub topics and its payloads against `HubAvailability`, and `nodes.ts` types its `online` as `NodeAvailability`. A misspelled topic or value now fails the typecheck; both were tried. `gen-contract.ts` refuses two text topics that would share a type name and names both; a new test feeds it a contract that does that. The existing Hub tests are unchanged and pass.

