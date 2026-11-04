# MQTT topics and payloads

What a Node publishes to the Broker. Topic names and payloads are the contract between Nodes and the Hub (ADR-0003: the Hub owns Home Assistant). This page explains it; the machine-readable contract is in [`contract/`](../contract/): the topic index `contract/topics.json`, a JSON Schema per JSON payload and example payloads. CI checks every publish in the core tests against it, and checks that this page covers every topic in the index.

The contract only grows (ADR-0005): topics and fields are only ever added, and an existing field never changes its meaning or type and is never removed. A change that can't be made by adding gets a new topic. Change `contract/` in the same commit as the firmware and Hub code that needs it.

## Connection

- MQTT 3.1.1 over plain TCP, clean session, keepalive 60 s, all publishes at QoS 0.
- Client ID: the Node ID.
- The host is a DNS name, a `.local` mDNS name or an IPv4 address.
- Username and password from the `node broker` settings; with no username the Node connects anonymously.
- Last will: `offline`, retained, on the availability topic.
- After a failed connect or a lost connection the Node connects again after 10 s. It gives up on a connect that hasn't finished after 30 s. Without Wi-Fi it doesn't try at all.

## Node ID

The Node's Wi-Fi MAC address as 12 lowercase hex digits, for example `404cca0ab1f2`. It never changes and needs no configuration. `node id` prints it.

## Topics

Every topic of a Node's own sits under `g2labs/<node-id>/`.

| Topic | Retained | Payload |
|---|---|---|
| `g2labs/<node-id>/availability` | yes | `online` or `offline` (plain text) |
| `g2labs/<node-id>/status` | no | status JSON, below |

### `availability`

`online` as soon as the Broker accepts the connection. The Broker publishes the last will `offline` when the Node drops off. Whenever the Node itself closes a working connection (Broker settings changed or cleared, Wi-Fi going away), it first publishes `offline` itself, because a clean disconnect doesn't fire the last will.

### `status`

Published right after `online`, then every 60 s while connected.

```json
{
  "node_id": "404cca0ab1f2",
  "name": "Kitchen",
  "fw_version": "0.1.0",
  "uptime_s": 83,
  "network": "home",
  "rssi": -61,
  "free_heap": 54321
}
```

| Field | Type | Meaning |
|---|---|---|
| `node_id` | string | The Node ID. |
| `name` | string | The friendly name (`node name set`). Left out when none is set. |
| `fw_version` | string | Firmware version, as in `VERSION`. |
| `uptime_s` | integer | Seconds since boot. |
| `network` | string | SSID of the current Known Network. |
| `rssi` | integer or `null` | RSSI of the Wi-Fi link in dBm; `null` when the Wi-Fi driver can't report it. |
| `free_heap` | integer | Free heap memory in bytes, summed over all heaps. |

Consumers must ignore fields they don't know: later tickets add fields (for example the Sighting drop count). The Node itself never sends a field that isn't in `contract/schemas/status.schema.json`.
