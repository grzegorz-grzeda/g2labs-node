# The MQTT contract only grows

Topics and payload fields are only ever added. An existing field never changes its meaning or type and is never removed. Consumers ignore fields they don't know. A change that can't be made by adding gets a new topic, which Nodes publish alongside the old one until no Node in the fleet needs the old one. Payloads carry no schema version. This is what lets a fleet run mixed firmware versions during a rollout, or with a Node offline or on Hold, without the Hub knowing which version publishes which format.

## Considered Options

- **A `schema_version` field the Hub branches on**: rejected. It invites breaking changes and keeps every old format alive in the Hub forever. The Hub can still read `fw_version` from Status if it ever has to.
- **Update every Node before changing the contract**: rejected. One Node that is offline or on Hold blocks the change.
