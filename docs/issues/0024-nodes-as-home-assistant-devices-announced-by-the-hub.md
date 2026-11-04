---
number: 24
title: "Nodes as Home Assistant devices, announced by the Hub"
status: open
labels: [ready-for-agent]
parent: 17
blocked_by: [23, 8]
created: 2026-11-04
---

# Nodes as Home Assistant devices, announced by the Hub

## What to build

Every Node appears in Home Assistant as a device, announced by the Hub (ADR-0003), with diagnostic sensors and Reboot and Publish status buttons that also work from the admin page.

## Acceptance criteria

- [ ] For each registered Node the Hub publishes HA discovery for one device keyed by the Node ID, named by its friendly name when set
- [ ] Diagnostic sensors: Wi-Fi RSSI, Known Network, uptime, firmware version, free memory, Sighting drop count, reading the Node's own Status topic
- [ ] Reboot and Publish status HA buttons publish straight to the Node's command topic (#8)
- [ ] Entity availability requires both `g2labs/hub/availability` and the Node's own availability to be `online`
- [ ] The discovery prefix is a Hub setting, default `homeassistant`; the Hub announces everything again when `<prefix>/status` reports `online`
- [ ] Forgetting a Node removes its HA device
- [ ] Reboot and Publish status buttons on the admin page's Node detail page
- [ ] Verified against a real Home Assistant: the device appears, the buttons work, and the entities go unavailable when the Hub is stopped
