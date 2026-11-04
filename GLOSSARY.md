# g2labs-node

Firmware for a generic, always-powered IoT node that gathers readings from attached and nearby sensors and exchanges data and commands with a self-hosted central system.

## Language

**Node**:
One device running g2labs-node firmware. Its attached hardware varies per installation and is configured, not hard-coded.
_Avoid_: Device, board, gateway

**Broker**:
The self-hosted MQTT server that Nodes publish to and receive commands from.
_Avoid_: Cloud, server, backend

**Known Network**:
A Wi-Fi network (SSID and password) stored on a Node that it is allowed to join.
_Avoid_: Saved network, profile, credential

**Sighting**:
One reception of a BLE advertisement by a Node: who sent it, how strongly it was heard, and what it said. Nodes report every Sighting; filtering and deduplication happen outside the Node.
_Avoid_: Scan result, detection, beacon, reading

**Aggregator**:
The service outside this repo that consumes Sightings from all Nodes via the Broker, deduplicating, throttling and positioning them.
_Avoid_: Collector, backend, processor
