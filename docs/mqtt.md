# MQTT topics and payloads

What Nodes and the Hub publish to the Broker. Topic names and payloads are the contract between Nodes and the Hub (ADR-0003: the Hub owns Home Assistant). This page explains it; the machine-readable contract is in [`contract/`](../contract/): the topic index `contract/topics.json`, a JSON Schema per JSON payload and example payloads. CI checks every publish in the core tests against it, and checks that this page covers every topic in the index.

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
| `g2labs/<node-id>/command` | no | command JSON, below; the Node subscribes, the Hub and Home Assistant publish |

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

### `command`

The Node subscribes to its command topic (QoS 0) every time it connects. Publish a command to it, not retained:

```json
{"command": "reboot"}
```

| `command` | What the Node does |
|---|---|
| `reboot` | Publishes `offline` on `availability`, disconnects, and reboots 1 s later. It comes back `online` as usual. |
| `publish_status` | Publishes its `status` at once. The periodic status then follows 60 s after it. |

The payload is a JSON object of at most 128 bytes. `contract/schemas/command.schema.json` lists the members there are today; new ones are added to it as optional members (ADR-0005), and the Node ignores members it doesn't know, so older Nodes keep working. It ignores and logs (on the UART console) every other message on the topic, and none of these can crash it:

- an unknown command name;
- a malformed payload: not a JSON object, no string `command` member, `command` given twice, or longer than 128 bytes;
- a retained message, so that a stray retained `reboot` can't reboot the Node every time it connects.

Commands get no reply; watch `availability` or `status` for the effect.

## The Hub

The Hub (`service/`) connects with client ID `g2labs-hub`, MQTT 3.1.1, clean session, keepalive 60 s, with the username and password from `HUB_MQTT_USERNAME` / `HUB_MQTT_PASSWORD` (anonymous without them). Its last will is `offline`, retained, on its availability topic. After a failed connect or a lost connection it connects again after `HUB_MQTT_RECONNECT_S` (default 5 s).

| Topic | Retained | Payload |
|---|---|---|
| `g2labs/hub/availability` | yes | `online` or `offline` (plain text), QoS 1 |

### `g2labs/hub/availability`

`online` as soon as the Broker accepts the Hub's connection. The Broker publishes the last will `offline` when the Hub drops off; when the Hub shuts down cleanly it publishes `offline` itself first. Every entity the Hub announces to Home Assistant will list this topic as an availability topic, so it all goes unavailable while the Hub is down.
