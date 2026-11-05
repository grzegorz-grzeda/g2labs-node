---
number: 39
title: "Take the Hub's Node topics and availability values from the contract"
status: open
labels: [ready-for-agent]
assignee: claude
parent: 17
blocked_by: [23]
created: 2026-11-05
---

# Take the Hub's Node topics and availability values from the contract

## What to build

A follow-up to #23, from the review of its PR. The Hub validates Status against `contract/`, but two things in `service/src/nodes.ts` are still hand-written copies of the contract:

- The availability values `online` / `offline`. `contract/topics.json` lists them as `values`, but `service/scripts/gen-contract.ts` leaves them out of the topics it generates.
- The Node topics it subscribes to (`g2labs/+/availability`, `g2labs/+/status`) and the regex that matches them. Forget already takes its retained topics from the generated topic list.

Behaviour stays the same.

## Acceptance criteria

- [ ] The generated contract module carries each text topic's `values` and a type for them
- [ ] The Hub checks availability against those values
- [ ] The Hub's subscriptions and topic matching come from the generated topics
- [ ] The existing Hub tests pass unchanged
