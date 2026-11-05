# g2labs-node

Firmware for a generic, always-powered IoT node that gathers readings from attached and nearby sensors, and the self-hosted Hub that manages and observes those nodes.

## Language

**Node**:
One device running g2labs-node firmware. Its attached hardware varies per installation and is configured, not hard-coded.
_Avoid_: Device, board, gateway

**Node ID**:
A Node's unique, unchanging identifier: its Wi-Fi MAC address as 12 lowercase hex digits. Used as the MQTT client ID and in the Node's topics. A Node can also have a friendly name, which is only a label.
_Avoid_: Device ID, serial, hostname

**mDNS name**:
The name a Node answers to on its local network, `g2labs-<node-id>.local`, so the Hub reaches it without knowing its IP address. Derived from the Node ID and just as unchanging.
_Avoid_: Hostname, DNS name

**Broker**:
The self-hosted MQTT server that Nodes publish to and receive commands from.
_Avoid_: Cloud, server, backend

**Known Network**:
A Wi-Fi network (SSID and password) stored on a Node that it is allowed to join.
_Avoid_: Saved network, profile, credential

**Provisioning**:
Setting a Node's friendly name, Known Networks and Broker from a computer over the Node's serial shell.
_Avoid_: Configuration, setup, onboarding

**Status**:
A Node's periodic report of its own health: firmware version, board, uptime, Known Network, IP address, link quality, memory.
_Avoid_: Telemetry, heartbeat, diagnostics

**Sighting**:
One reception of a BLE advertisement by a Node: who sent it, how strongly it was heard, and what it said, undecoded. Nodes report every Sighting; filtering, deduplication and decoding happen in the Hub.
_Avoid_: Scan result, detection, beacon, reading, telemetry

**Hub**:
The self-hosted service that manages and observes all Nodes through the Broker. Home Assistant is one of its integrations, not its master.
_Avoid_: Server, backend, g2labs-node-service (that is the package name, not the concept)

**Aggregator**:
The part of the Hub that consumes Sightings from all Nodes, deduplicating, throttling and positioning them.
_Avoid_: Collector, backend, processor

**Advertiser**:
Any BLE transmitter the Hub has seen in Sightings, identified by its address. Transient: forgotten after a period of silence.
_Avoid_: Device, beacon, tag

**Sensor**:
An Advertiser whose data the Hub can decode. It starts Discovered; once the user adopts it, the Hub tracks it and exposes it to its integrations, marks it Unavailable while no Node hears it, and forgets it when Removed.
_Avoid_: Device, beacon, tag, TH sensor (a TH sensor is just one kind of Sensor)

**Update Policy**:
The fleet-wide rule for how the Hub updates Node firmware: Manual (the user picks an image and a Node) or Follow Releases (the Hub rolls out each new stable release to every Node).
_Avoid_: OTA mode, update strategy

**Hold**:
A per-Node flag that excludes the Node from automatic rollouts under the Update Policy. Manual updates still apply.
_Avoid_: Pin, freeze, exclude

**Release**:
A numbered version of the Node firmware and the Hub, published together for the fleet. A Release is stable unless it is a Release Candidate. Only a stable Release is rolled out under Follow Releases; work merged into the codebase is not a Release.
_Avoid_: Build, deploy, update, merge

**Release Candidate**:
A Release published for testing on real hardware before a stable Release of the same version. Follow Releases skips it unless the user allows Release Candidates.
_Avoid_: Pre-release, beta, rc build

**Rollout**:
Updating every eligible Node to one Release, one Node at a time, stopping at the first failure.
_Avoid_: Deployment, OTA, FOTA, push
