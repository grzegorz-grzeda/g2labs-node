# The Hub owns everything Home Assistant sees, and all decoding

Nodes publish only raw Sightings and their own Status to the Broker. They never publish Home Assistant discovery and never decode advertisements. The Hub decodes Sightings, turns the Advertisers the user adopts into Sensors with a lifecycle, and is the only publisher of HA discovery, for Sensors and for Nodes themselves. HA is one integration of the Hub, not the place where the system is defined. This supersedes [ADR-0002](0002-every-node-announces-th-sensors-to-home-assistant.md) and follows [ADR-0001](0001-nodes-report-every-sighting-unfiltered.md) to its end: anything that needs a view across Nodes, or a policy, lives in the Hub.

## Considered Options

- **Nodes announce TH sensors to HA until the Hub takes over (ADR-0002)**: rejected. Every Node announced every decodable sensor in range, including a neighbour's, and Nodes would need a handover protocol.
- **Nodes keep their own HA device, the Hub takes Sensors**: rejected in favour of one owner for the HA side and no HA code in firmware.
- **BTHome decoding on the Node alongside the raw payload**: rejected. It is a second decoder that can disagree with the Hub's, and new formats would need reflashing.

## Consequences

- With the Hub down, nothing new reaches HA: Node and Sensor entities go unavailable. Nodes keep running and the Broker keeps their retained availability.
- Spec issue #1's Node-side HA work (tickets #11, #12, #13, and the discovery-prefix and TH-announcing settings) is dropped from the firmware and re-planned as Hub work.
- Sensors reach HA only after the user adopts them, so neighbours' sensors stay out of HA.
