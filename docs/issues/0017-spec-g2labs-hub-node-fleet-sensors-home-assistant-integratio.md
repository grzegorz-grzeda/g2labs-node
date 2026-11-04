---
number: 17
title: "Spec: g2labs Hub — Node fleet, Sensors, Home Assistant integration, firmware updates"
status: open
labels: [ready-for-agent]
parent:
blocked_by: []
created: 2026-11-04
---

# Spec: g2labs Hub — Node fleet, Sensors, Home Assistant integration, firmware updates

## Problem Statement

Nodes (spec #1) publish raw Sightings and their Status to the Broker, and that's all. Nothing turns those Sightings into the sensors I care about, nothing keeps Node history, nothing updates firmware across the fleet, and nothing shows the fleet in one place. Home Assistant shouldn't be where the system is defined: it is one integration among possibly several.

## Solution

The **Hub** (package `g2labs-node-service`, in `service/` of this repo): a self-hosted service running as a Docker container on a Raspberry Pi next to Mosquitto and Home Assistant. It:

- registers every Node it sees on the Broker and keeps its Status history;
- runs the **Aggregator**: takes in Sightings from all Nodes, decodes them, and tracks the Advertisers you adopt as **Sensors** with a lifecycle;
- is the only thing that talks to Home Assistant, announcing Nodes and Sensors through MQTT discovery (ADR-0003);
- updates Node firmware over SMP-UDP under a fleet-wide **Update Policy** (Manual or Follow Releases), with a per-Node **Hold**;
- serves an admin web page for all of it.

Terms follow `GLOSSARY.md`: Node, Status, Sighting, Hub, Aggregator, Advertiser, Sensor, Update Policy, Hold.

## User Stories

### Nodes

1. As a home owner, I want every Node that appears on the Broker registered automatically, so that I don't add Nodes by hand.
2. As a maintainer, I want a list of Nodes with availability, friendly name, firmware version, board, Known Network, RSSI and last seen, updating live, so that I see the fleet's health at a glance.
3. As a maintainer, I want a Node's Status history (about 30 days), so that I can see RSSI, memory and reconnects over time.
4. As a maintainer, I want Reboot and Publish status buttons for a Node in the admin page, so that I can act on it without HA.
5. As a maintainer, I want to Forget a Node, deleting its history and clearing its retained topics, so that retired Nodes disappear.

### Sightings and Sensors

6. As the Aggregator, I want to take in every Sighting batch from every Node, so that I see the whole BLE picture.
7. As a maintainer, I want a live list of Advertisers (address, which Nodes hear them, RSSI, decoded data when understood), so that I can find the things worth tracking.
8. As a maintainer, I want the drop counts Nodes report to be visible, so that I know when Sightings were lost.
9. As a home owner, I want BTHome v2 advertisements decoded by the Hub, so that TH sensors give temperature, humidity and battery.
10. As a home owner, I want an Advertiser the Hub can decode shown as Discovered, with live values, and kept out of HA until I adopt it, so that my neighbour's sensors don't reach HA.
11. As a home owner, I want to Adopt a Discovered Advertiser and give it a name, so that it becomes a Sensor that the Hub tracks and announces.
12. As a home owner, I want an optional switch to adopt decodable Advertisers automatically (off by default), so that a trusted environment needs no clicks.
13. As a home owner, I want a Sensor marked Unavailable after a configurable silence (default 15 min), so that HA doesn't show stale values as live.
14. As a home owner, I want to Remove a Sensor, deleting it from HA and dropping its history, so that retired sensors go away cleanly.
15. As the Aggregator, I want one advertisement heard by several Nodes counted once (same address and payload within about 2 s, using the BTHome packet ID where present), with the strongest reception recorded as "heard by", so that readings aren't duplicated.
16. As a developer, I want decoders as pluggable modules, so that formats like ATC/pvvx or Xiaomi can be added later without touching the rest of the code or the firmware.

### Home Assistant

17. As a Home Assistant user, I want every Node to appear as an HA device with diagnostic sensors (RSSI, Known Network, uptime, firmware version, free memory, drop count), so that I monitor Nodes from HA.
18. As a Home Assistant user, I want Reboot and Publish status buttons on a Node's HA device that publish straight to the Node's command topic, so that I can act on a Node from HA.
19. As a Home Assistant user, I want every adopted Sensor to appear as an HA device with its decoded entities, with device and entity IDs derived from the sensor's MAC, so that IDs stay stable.
20. As a Home Assistant user, I want a Sensor's state published when a value changes, or at least every few minutes, not on every advertisement, so that HA and the Broker aren't flooded.
21. As a Home Assistant user, I want every entity the Hub announces to go unavailable when the Hub is down, so that I never see stale values as live.
22. As a Home Assistant user, I want the Hub to announce everything again when HA publishes `online` to `<prefix>/status`, so that entities come back after an HA restart.
23. As a Home Assistant user, I want the discovery prefix configurable (default `homeassistant`), so that the Hub works with a non-default HA setup.

### Firmware updates

24. As a maintainer, I want to upload a signed image, say which board it's for, and update a chosen Node with it (downgrades allowed), so that I can test builds on one Node. This is the Manual Update Policy, and it always works, even on a Node on Hold.
25. As a maintainer, I want the Hub to follow this repo's GitHub Releases and roll each new stable release out to every Node on an older version, so that the fleet stays current without clicks. This is the Follow Releases Update Policy.
26. As a maintainer, I want pre-releases skipped unless I turn on a switch, so that experiments don't reach the whole fleet.
27. As a maintainer, I want a rollout to update one Node at a time, judge success only by the Node coming back `online` with the new `fw_version` within a timeout, and stop at the first failure without retrying that version automatically, so that a bad release reaches at most one Node.
28. As a maintainer, I want to put a Node on Hold, so that automatic rollouts skip it (for example a Node on my desk with a dev build).
29. As a maintainer, I want the Hub to pick each Node's image from the release manifest by the board the Node reports in Status, and to refuse a manual upload whose board doesn't match, so that an image never goes to the wrong board.
30. As a maintainer, I want the Hub to reach Nodes by their mDNS name `g2labs-<node-id>.local`, falling back to the `ip` in Status, so that updates work with DHCP and with flaky Wi-Fi multicast.
31. As a maintainer, I want to see each update's progress and outcome and a history of updates, so that I know what happened.

### Running the Hub

32. As a home owner, I want the Hub as a multi-arch (`arm64` and `amd64`) Docker image on GHCR, with an example `docker-compose.yml` next to Mosquitto, so that it installs on my Raspberry Pi like everything else.
33. As a home owner, I want the Hub configured through env vars (Broker URL and credentials, admin password, data path), with its database on a volume, so that upgrading the container keeps my data.
34. As a home owner, I want the admin page protected by one admin password, so that other people on my LAN don't change things by accident.
35. As a maintainer, I want the Hub to publish its own retained `g2labs/hub/availability` with a last will, so that its consumers know when it's down.

## Implementation Decisions

- **Repo and contract (ADR-0004).** The Hub lives in `service/`. The MQTT contract lives in `contract/` as one JSON Schema per payload plus example payloads. Firmware core tests check that published payloads validate against the schemas. The Hub's TypeScript types are generated from the schemas, its tests use the examples, and it validates every incoming message at runtime (invalid ones are logged and counted, never crash it). `docs/mqtt.md` stays the human description and links to the schemas.
- **Contract evolution (ADR-0005).** Fields and topics are only added. The Hub ignores unknown fields and doesn't branch on firmware version.
- **Stack.** TypeScript on Node.js. SQLite via `better-sqlite3` in WAL mode. React + Vite single-page admin page, built to static files and served by the Hub, with live updates pushed over WebSocket or SSE. Our own small SMP-UDP client (8-byte SMP header plus CBOR, for image upload, image list and reset), because no Node.js SMP-UDP library exists. The Hub does mDNS resolution itself.
- **Storage.** SQLite holds the Node registry, Status history (about 30 days, pruned), Sensors and their last values, firmware images, update jobs and settings. Raw Sightings and Advertisers stay in memory only (a short rolling window) and never touch the SD card.
- **Hub topics.** `g2labs/hub/availability` (retained, last will `offline`). Sensor state on `g2labs/sensor/<sensor-id>/state`, with `<sensor-id>` derived from the sensor's MAC. HA discovery under `<prefix>/<component>/<id>/<object>/config`. Every Hub-announced entity lists `g2labs/hub/availability` as an availability topic, plus the Node's own availability for Node entities. These topics are part of `contract/` and `docs/mqtt.md`.
- **Update engine.** Both Update Policies share one engine: resolve the Node (mDNS, then Status `ip`), upload over SMP-UDP, mark as test, reset, wait for `online` with the expected `fw_version` (the Node confirms its own image after reaching the Broker, #15), time out as failed (the Node reverts by itself). Follow Releases polls the public GitHub Releases API every few hours and uses only the release's `manifest.json` (board → asset and SHA-256), never asset file names.
- **Deployment.** One container, `network_mode: host` (needed for mDNS and SMP-UDP to Nodes), config through env vars, data on a volume. CI builds and tests the Hub on every push and publishes the multi-arch image to GHCR on tags.
- **Admin access.** One admin password from an env var, session cookie. No user accounts.

## Testing Decisions

- **What a good test is.** Tests drive the Hub only through its boundaries (MQTT messages in, HTTP/WebSocket API calls, a fake SMP endpoint, a fake GitHub Releases API, the clock) and assert only on what comes out (MQTT publishes, API responses, SMP requests). No test reaches into module internals.
- **Contract tests.** Every example in `contract/examples/` validates against its schema and is accepted by the Hub. CI fails if firmware core tests publish a payload that doesn't validate.
- **Aggregator scenarios.** The same advertisement from three Nodes gives one reading with the strongest Node recorded; BTHome samples decode correctly; malformed payloads are ignored; Discovered → Adopted → Unavailable → Removed produces the right HA discovery and clears.
- **Update scenarios** against a fake SMP endpoint and a fake Node publishing Status: success, timeout leading to failure and stopping the rollout, Hold skipping a Node, board mismatch refused, pre-release skipped.
- **Optional end-to-end.** The Hub against a real Mosquitto in Docker, fed with recorded Node traffic.

## Out of Scope

- Positioning moving tags, and presence based on position. Moving tags can be adopted, just without a position.
- Decoders other than BTHome v2, and encrypted BTHome.
- Integrations other than Home Assistant.
- Provisioning (stays in `scripts/g2labs-node.py` over the serial shell).
- Authentication for SMP-UDP, MQTT over TLS, signed images with a real key (the security work).
- Multiple admin users, or remote access from outside the LAN.
- Packaging as a Home Assistant add-on.

## Further Notes

- Decisions: ADR-0003 (the Hub owns HA and decoding; supersedes ADR-0002), ADR-0004 (one repo with a shared contract), ADR-0005 (the contract only grows).
- Firmware prerequisites, tracked under #1: Sighting batches (#9), MQTT commands (#8), SMP-UDP (#14), safe image confirmation (#15), `board` and `ip` in Status, mDNS hostname, and a release workflow publishing per-board images with a manifest.
