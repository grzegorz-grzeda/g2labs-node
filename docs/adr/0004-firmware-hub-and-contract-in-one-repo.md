# Firmware, Hub and their contract live in one repo

The Hub (Node.js, in `service/`) lives in this repository next to the firmware, with the MQTT contract in `contract/` as JSON Schemas plus example payloads. Firmware core tests check that the payloads the core publishes validate against the schemas. Hub tests use the same examples as input. A contract change is then one PR, checked by one CI run, on both sides at once.

## Considered Options

- **Separate repos with a versioned contract package**: rejected for now. With one developer and a contract still changing, every change would need two PRs landing in sync. Worth revisiting once the contract settles.

## Consequences

- The repo is no longer just a Zephyr app: CI gains a Node.js job, and the repo's name undersells its contents.
