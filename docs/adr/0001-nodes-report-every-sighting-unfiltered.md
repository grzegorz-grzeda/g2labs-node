# Nodes report every Sighting unfiltered

A Node publishes every BLE advertisement it hears as a Sighting, from any device and always with the raw payload, and does no allowlisting, deduplication or throttling. All of that belongs to the Aggregator, which sees Sightings from every Node at once and is the only place where cross-Node work such as positioning a moving BLE tag or picking the best reception of a sensor can happen. The Node's only volume control is batching Sightings into one message per time window, with a bounded buffer that drops the oldest and reports a drop count, so the Aggregator always knows when it has missed data.

## Considered Options

- **Allowlist of known sensors on the Node**: rejected. It hides devices from the Aggregator (moving tags, new sensors) and means reconfiguring every Node to start tracking something.
- **Deduplicating or throttling on the Node**: rejected. A single Node can't tell a duplicate from a meaningful repeat, and per-Node RSSI over time is exactly what positioning needs.

## Consequences

- Message volume grows with the BLE noise around a Node, so batch size, buffer size and the MQTT payload format (JSON now, possibly CBOR later) are the levers if Wi-Fi airtime or Broker load becomes a problem. Filtering on the Node is not one of those levers.
- Decoding (e.g. BTHome) on the Node is a convenience added alongside the raw payload. The Aggregator must never depend on it.
