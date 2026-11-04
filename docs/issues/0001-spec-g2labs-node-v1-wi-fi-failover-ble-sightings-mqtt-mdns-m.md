---
number: 1
title: "Spec: g2labs-node v1 — Wi-Fi failover, BLE Sightings, MQTT, mDNS, MCUboot/SMP updates"
status: open
labels: [ready-for-agent]
parent:
blocked_by: []
created: 2026-11-04
---

# Spec: g2labs-node v1 — Wi-Fi failover, BLE Sightings, MQTT, mDNS, MCUboot/SMP updates

## Problem Statement

I want a fleet of small, always-powered Nodes around my home that listen to everything on BLE (temperature/humidity sensors, tags and other devices) and report it to my self-hosted Broker. From there, the Hub (#17) turns Sightings into Sensors, keeps the fleet's history, updates firmware and presents everything to Home Assistant (ADR-0003). Today there is nothing: the repository is an empty Zephyr application. Each Node needs to stay online without hand-holding (multiple Wi-Fi networks, failover), be maintainable remotely (shell, settings, image updates that can't brick it), and run on the ESP32-C6, with the ESP32-S3 as a fallback.

## Solution

A Zephyr firmware for ESP32-C6 (primary) and ESP32-S3 (fallback), booted by MCUboot, that:

- stores up to 5 Known Networks, joins the strongest one, and fails over when the network or the Broker becomes unreachable;
- connects to one Broker over MQTT (host, port, username and password from settings) and keeps a retained online/offline status plus a periodic status message;
- passively scans BLE and reports every advertisement as a raw, undecoded Sighting, in timed batches;
- answers Reboot and Publish status commands over MQTT, and is reachable by the mDNS name `g2labs-<node-id>.local`;
- leaves decoding, Sensors and everything Home Assistant sees to the Hub (ADR-0003);
- is configured and maintained through a UART shell and through SMP over UDP (shell, settings and image upload), confirming a new image only once it has reached the Broker.

## User Stories

### Provisioning and settings

1. As an installer, I want to add a Known Network (SSID and password) through the UART shell, so that a freshly flashed Node can get online.
2. As an installer, I want to list Known Networks through the shell without the passwords shown, so that I can check what a Node knows without exposing secrets.
3. As an installer, I want to remove a Known Network through the shell, so that I can retire networks that no longer exist.
4. As an installer, I want adding a 6th Known Network to be rejected with a clear message, so that I know the limit is 5 and must remove one first.
5. As an installer, I want to set the Broker host and port through the shell, so that the Node knows where to connect.
6. As an installer, I want to set the Broker username and password through the shell, so that the Node can connect to a Home Assistant Mosquitto broker that rejects anonymous clients.
7. As an installer, I want to give a Node an optional friendly name, so that I can tell Nodes apart in Home Assistant and in Broker topics' payloads.
8. As an installer, I want every setting to survive reboots and image updates, so that I configure a Node only once.
9. As an installer, I want new settings to take effect without reflashing, and to be told when a reboot or reconnect is needed, so that changing a Broker or network is quick.
10. _(moved to the Hub, #17: HA discovery prefix)_
11. _(dropped by ADR-0003: Nodes don't announce TH sensors)_
12. As an installer, I want the BLE scan duty cycle to be a setting, so that I can trade Sighting coverage against Wi-Fi airtime.
13. As a maintainer, I want to read and change settings over SMP-UDP, so that I can reconfigure a Node without a cable.

### Wi-Fi

14. As a home owner, I want a Node to join the strongest reachable Known Network at boot, so that it gets the best link available.
15. As a home owner, I want a Node to stay on its current Known Network while it is healthy, even when a stronger one appears, so that it doesn't churn connections.
16. As a home owner, I want a Node to retry its current Known Network when the link drops, so that a brief Wi-Fi glitch doesn't trigger a network change.
17. As a home owner, I want a Node to rescan and move to the strongest other Known Network when it can't reconnect or gets no IP address within a timeout, so that it recovers from a dead access point by itself.
18. As a home owner, I want a Node to move to another Known Network when the Broker has been unreachable for several minutes, so that a network with no upstream connection doesn't strand it.
19. As a home owner, I want a Node with no reachable Known Network to keep rescanning periodically, so that it comes back once any Known Network returns.
20. As a maintainer, I want the shell to show the current Known Network, its RSSI and the IP address, so that I can diagnose connectivity.

### Broker and status

21. As a home owner, I want a Node to connect to the Broker and reconnect automatically after any disconnect, so that data keeps flowing.
22. As a Home Assistant user, I want a Node's retained status to say `online` once it connects and `offline` (via last will) when it drops off, so that the Hub and HA show its availability.
23. As a maintainer, I want each Node to identify itself by an ID derived from its Wi-Fi MAC address, so that IDs are unique and stable without configuration.
24. As a maintainer, I want all of a Node's own topics under `g2labs/<node-id>/…`, so that I can subscribe to one Node or all of them easily.
25. As a maintainer, I want a status message every 60 s with firmware version, board, uptime, current Known Network and its RSSI, IP address, free memory and the Sighting drop count, so that I can monitor Node health and the Hub can pick the right image and reach the Node.
26. As a maintainer, I want to send a Reboot command over MQTT, so that I can restart a Node remotely.
27. As a maintainer, I want to send a Publish status command over MQTT, so that I get fresh status immediately instead of waiting up to 60 s.
28. As a maintainer, I want unknown or malformed MQTT commands to be ignored and logged, so that a bad message can't crash a Node.

### BLE Sightings

29. As the Aggregator, I want a Sighting for every BLE advertisement a Node hears, from any device, so that I can position moving tags and pick the best reception myself.
30. As the Aggregator, I want each Sighting to carry the advertiser address and address type, the RSSI and the raw advertisement payload, so that I never depend on the Node's decoding.
31. _(dropped by ADR-0003: the Hub decodes BTHome from the raw payload)_
32. As the Aggregator, I want each Sighting timestamped in epoch milliseconds once the Node has synced time with SNTP, so that I can compare Sightings across Nodes.
33. As the Aggregator, I want Sightings taken before time sync to carry uptime and a "not synced" flag rather than being held back, so that no data is lost and I know the timestamp isn't absolute.
34. As the Aggregator, I want Sightings delivered in one batched message per time window (about 1 s) per Node, so that the Broker and the Node's Wi-Fi aren't swamped.
35. As the Aggregator, I want a batch to contain every Sighting from its window, with no deduplication on the Node, so that per-Node RSSI over time is preserved.
36. As the Aggregator, I want a drop count in each batch and in the status message whenever the Node's bounded buffer overflowed (oldest dropped first), so that I know when I'm missing data.
37. As the Aggregator, I want batches to keep coming while the Node is offline from the Broker, only for the buffer's capacity, with drops counted, so that the Node's memory stays bounded.
38. As a home owner, I want BLE scanning to be passive, so that Nodes don't send scan requests to every device around.
39. As a Home Assistant user, I want raw Sightings never published as HA entities, so that HA isn't flooded.

### Home Assistant

40–48. _(moved to the Hub, #17, by ADR-0003: the Hub announces Nodes and Sensors to Home Assistant; Nodes never publish HA discovery)_

### Discovery and releases

60. As the Hub, I want each Node to answer mDNS as `g2labs-<node-id>.local`, so that I can reach it for updates without knowing its IP address.
61. As the Hub, I want every tagged release to carry one signed image per board plus a `manifest.json` mapping each board to its asset and SHA-256, so that I can update every Node with the right image.
62. As a developer, I want the core tests to check that every payload the core publishes validates against the JSON Schemas in `contract/`, so that the firmware can't drift from what the Hub expects (ADR-0004).

### Image updates and resilience

49. As a maintainer, I want the firmware booted by MCUboot, so that images can be updated and reverted safely.
50. As a maintainer, I want to upload a new image over SMP-UDP, so that I can update Nodes without a cable.
51. As a maintainer, I want a new image to run as a test and confirm itself only once it has joined a Known Network and connected to the Broker, so that an image that breaks networking reverts and stays reachable for another update.
52. As a maintainer, I want a hardware watchdog, so that an image that hangs reboots and, if unconfirmed, reverts.
53. As a maintainer, I want to list images and their state (active, pending, confirmed) over SMP and the shell, so that I know what each Node is running.
54. As a maintainer, I want the firmware version reported in status and in HA, so that I can check an update landed.
55. As a maintainer, I want shell commands over SMP-UDP, so that I can run diagnostics remotely.

### Platform

56. As a developer, I want the firmware to build for `esp32c6_devkitc/esp32c6/hpcore` with MCUboot through sysbuild, so that the primary target is always buildable.
57. As a developer, I want the firmware to build for `esp32s3_devkitc/esp32s3/procpu` and pass the same hardware smoke test, so that the fallback stays usable.
58. As a developer, I want board-specific differences in board overlays and board config, not in shared logic, so that the core stays portable.
59. As a developer, I want the Node's core logic tested on `native_sim` in CI, so that behaviour is checked without hardware.

## Implementation Decisions

- **Two targets, one firmware.** ESP32-C6 (HP core) is primary and sets all defaults and tuning. ESP32-S3 (PRO CPU) is a fallback whose bar is: it builds and passes the hardware smoke test. Built with sysbuild and MCUboot using MCUboot's development signing key. No image encryption and no partition headroom reserved for it; a real key and encryption come later and will require reflashing over a cable (accepted).
- **Core vs adapters.** All decision-making lives in a Node core that never touches Zephyr radio/network APIs directly. Thin platform adapters translate between Zephyr subsystems and the core:
  - Wi-Fi adapter (Zephyr Wi-Fi management): emits scan results, connected, disconnected, got-IP events; accepts connect/scan requests.
  - BLE adapter (Zephyr Bluetooth observer, passive scan): emits advertisement reports (address, address type, RSSI, raw payload).
  - MQTT adapter (Zephyr MQTT client over plain TCP with username/password): emits connection state and incoming messages; accepts publishes (topic, payload, retained) and a last will.
  - Time adapter: uptime and SNTP sync state/epoch time.
  - Image adapter: MCUboot "confirm this image".
  - Settings adapter: Zephyr settings subsystem.
  The core's inputs are those events plus settings; its outputs are MQTT publishes, Wi-Fi requests and the image-confirm call. This boundary is the single test seam (see Testing Decisions).
- **Core modules** (each behind a small interface inside the core):
  - *Network selector*: owns Known Networks (max 5) and the policy: join strongest on boot; stay while healthy; retry current on link drop; after reconnect/DHCP timeout rescan and move to strongest other; after the Broker is unreachable for a sustained period (several minutes, configurable) rescan and move; with none reachable rescan periodically. Timeouts are constants or settings, tuned on the C6.
  - *Sighting batcher*: bounded ring buffer of Sightings, flushed as one message per window (about 1 s). On overflow, drop oldest and increment a drop count reported in the next batch and in status. No filtering or deduplication (ADR-0001).
  - *Message encoder*: JSON payloads for batches and status, matching the schemas in `contract/`.
  - *Command handler*: Reboot and Publish status; anything else is ignored and logged.
  - *Image-confirm policy*: confirms a pending test image after the first successful Broker connection.
- **Identity and topics.** Node ID is derived from the Wi-Fi MAC; an optional friendly name is a setting. Node topics sit under `g2labs/<node-id>/…`, covering at least: a retained online/offline status (last will publishes offline), periodic status (every 60 s), Sighting batches, and a command topic. Exact subtopic names and JSON field names are decided in the first ticket that defines them and then become a contract with the Aggregator.
- **No Home Assistant on the Node.** Nodes never publish HA discovery or decode advertisements; the Hub does both (ADR-0003). The MQTT contract only grows (ADR-0005).
- **mDNS name.** `CONFIG_NET_HOSTNAME="g2labs-"` with `CONFIG_NET_HOSTNAME_UNIQUE` and the mDNS responder give `g2labs-<node-id>.local`. Status also carries `ip` as a fallback for when Wi-Fi multicast is unreliable.
- **Releases.** A workflow on `v*` tags builds both boards and attaches `g2labs-node-<version>-<board>.signed.bin` per board plus `manifest.json`.
- **Sighting content.** Address and address type, RSSI, raw payload, and a timestamp: epoch milliseconds once SNTP has synced, otherwise uptime plus a "not synced" flag.
- **BLE scanning.** Passive, continuous, with the duty cycle configurable (start around 50%) so Wi-Fi keeps airtime on the shared radio.
- **Settings stored on the Node** (persisting across reboots and image updates): Known Networks, Broker host/port/username/password, friendly name, scan duty cycle. Passwords are never printed by the shell or returned by settings reads.
- **Management.** UART shell for all configuration and diagnostics. SMP over UDP with the shell, settings and image management groups. SMP-UDP is unauthenticated until the security work (accepted known gap).
- **Watchdog.** A hardware watchdog fed by the main loop, so a hung image reboots and, if unconfirmed, reverts.
- **One Broker, one connection.** The Hub and Home Assistant share the same Broker; each Node holds exactly one MQTT connection.

## Testing Decisions

- **What a good test is.** Tests drive the Node core only through its inputs (Wi-Fi events, Broker connection state, advertisement reports, time, incoming commands, settings) and assert only on its outputs (MQTT publishes with topic, payload and retained flag; Wi-Fi requests; image confirmation). No test reaches into a module's internals, so modules can be reshaped freely behind the core boundary.
- **Seam 1: Node core on `native_sim`, ztest under twister, run in CI.** Fake adapters feed events and record outputs. Representative scenarios:
  - Given Known Networks and a scan, the Node connects to the strongest; stays put when a stronger one appears while healthy; retries on link drop; moves after a reconnect/DHCP timeout; moves after the Broker is unreachable for the configured period; rescans when none are reachable; rejects a 6th Known Network.
  - Advertisements in, a batch per window out, containing every Sighting with raw payload, RSSI and address; overflow drops oldest and reports the drop count; timestamps switch from uptime + "not synced" to epoch after SNTP sync.
  - On Broker connect: retained `online` status, status every 60 s; last will configured as `offline`.
  - Every published payload validates against its schema in `contract/`.
  - Reboot and Publish status commands act; unknown/malformed commands are ignored.
  - A pending image is confirmed only after the first Broker connection, never before.
- **Seam 2: hardware smoke test, identical for C6 and S3, opt-in (pytest under twister with a hardware map).** Flash via sysbuild with MCUboot, provision a Known Network and Broker through the UART shell, verify the Node goes `online`, answers on its mDNS name, Sighting batches arrive, then upload a new image over SMP-UDP and verify it boots and confirms. This covers the thin adapters, which aren't tested on `native_sim`.
- **Not tested separately:** individual core modules get no unit seams of their own; ESP32 QEMU variants aren't used because they emulate neither Wi-Fi nor BLE.
- **Prior art.** None in this repo yet. Follow Zephyr's own ztest/twister conventions (`testcase.yaml`, `native_sim` platform) and pytest-twister-harness for the hardware test. The `smp_svr` and Wi-Fi `shell` samples in the Zephyr tree are reference material for the adapters.

## Out of Scope

- Security: MCUboot signing with a real key, image encryption, MQTT over TLS with certificates, and authentication for SMP.
- HTTP configuration web page, SoftAP fallback for provisioning, BLE provisioning, mDNS discovery of the Broker.
- CBOR or other binary encodings.
- Wired (I2C/GPIO) sensors and actuators.
- MQTT commands beyond Reboot and Publish status (settings or shell over MQTT).
- Battery operation and deep sleep.
- The Hub (#17), and any filtering, deduplication, throttling or decoding on the Node (ADR-0001, ADR-0003).
- Home Assistant discovery from the Node (ADR-0003).
- Roaming to a stronger Known Network while the current one is healthy.

## Further Notes

- Domain terms follow `GLOSSARY.md`: Node, Node ID, Broker, Known Network, Provisioning, Status, Sighting, Hub, Aggregator.
- Decisions recorded as ADRs: ADR-0001 (Nodes report every Sighting unfiltered), ADR-0002 (superseded), ADR-0003 (the Hub owns HA and decoding), ADR-0004 (one repo with a shared contract), ADR-0005 (the contract only grows).
- The ESP32-C6 and S3 share one radio between Wi-Fi and BLE; scan duty cycle and batch window are the knobs to tune once Wi-Fi throughput is measured on the C6.
- The Espressif HAL needs `west blobs fetch hal_espressif` once per workspace before building.
