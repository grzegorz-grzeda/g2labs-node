---
number: 25
title: "Sighting intake and live Advertisers in the Hub"
status: open
labels: [ready-for-agent]
parent: 17
blocked_by: [22, 9]
created: 2026-11-04
---

# Sighting intake and live Advertisers in the Hub

## What to build

The Aggregator's intake: the Hub takes in every Sighting batch from every Node, keeps a short rolling window in memory, and shows the Advertisers it sees.

## Acceptance criteria

- [ ] Subscribes to every Node's Sighting batches and validates them against `contract/`
- [ ] Sightings and Advertisers are kept in memory only, in a rolling window (a few minutes); nothing is written to SQLite
- [ ] An Advertiser silent for longer than the window is forgotten
- [ ] One advertisement heard by several Nodes (same address and payload within about 2 s) counts once, with every reception kept and the strongest recorded as "heard by"
- [ ] Advertisers page: address, address type, which Nodes hear it and at what RSSI, last seen, updated live
- [ ] The drop counts Nodes report in batches and in Status are shown per Node
- [ ] Memory stays bounded under a synthetic load of thousands of Advertisers with rotating addresses
