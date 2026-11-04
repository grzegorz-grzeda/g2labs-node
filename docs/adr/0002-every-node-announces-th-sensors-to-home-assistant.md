---
status: superseded by ADR-0003
---

# Every Node announces TH sensors to Home Assistant under a sensor-derived ID

Each Node that decodes a BTHome temperature/humidity sensor publishes Home Assistant MQTT discovery for it, using a device and entity ID derived from the sensor's MAC address rather than the Node's. Because every Node sends an identical announcement, HA merges them into one device, and whichever Node heard the sensor most recently updates its values. This makes TH sensors appear in HA without the Aggregator existing yet, while respecting [ADR-0001](0001-nodes-report-every-sighting-unfiltered.md): Nodes still don't coordinate or deduplicate between themselves. A Node setting turns the behaviour off, so the Aggregator can take over announcing sensors later.

## Considered Options

- **The Aggregator announces sensors after deduplication**: cleaner, but no TH sensors reach HA until the Aggregator is built.
- **The Node announces only listed sensors**: rejected, because it brings back the on-Node allowlist that ADR-0001 rules out.

## Consequences

- Any BTHome sensor in range of any Node, including a neighbour's, appears in HA until disabled there.
- The sensor-derived ID scheme is a contract with users' HA setups. Changing it, or moving announcing to the Aggregator under different IDs, renames entities and breaks automations and history, so a future Aggregator must reuse the same IDs.
- Sensor values in HA can come from different Nodes over time, so a per-reading RSSI or "heard by" attribute reflects the last reporter, not a stable link.
