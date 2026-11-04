---
number: 22
title: "Hub walking skeleton: Broker connection, admin page, Docker image"
status: closed
closed_reason: completed
closed: 2026-11-04
labels: [ready-for-agent]
parent: 17
blocked_by: []
created: 2026-11-04
---

# Hub walking skeleton: Broker connection, admin page, Docker image

## What to build

The thinnest Hub that runs end to end: a TypeScript service in `service/` that connects to the Broker, announces itself, serves a password-protected admin page, and ships as a multi-arch Docker image.

## Acceptance criteria

- [x] `service/` holds a TypeScript Node.js service and a React + Vite admin page that it builds and serves itself
- [x] Configured by env vars: Broker URL, username and password, admin password, data directory
- [x] Connects to the Broker and publishes retained `online` on `g2labs/hub/availability`, with a last will `offline`; reconnects after a lost connection
- [x] The admin page requires the admin password and keeps a session cookie; it shows whether the Hub is connected to the Broker
- [x] Opens a SQLite database (WAL mode) in the data directory, with schema migrations
- [x] A Dockerfile builds an `arm64` and `amd64` image; `service/docker-compose.example.yml` runs it with `network_mode: host` next to Mosquitto
- [x] CI builds and tests the Hub on every push and publishes the image to GHCR on Releases: tagged with the version for a Release Candidate, with the version and `latest` for a stable Release
- [x] `g2labs/hub/availability` is documented in `docs/mqtt.md` and has a schema in `contract/`
- [x] CLAUDE.md describes `service/` and its commands
