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
- [x] CI builds and tests the Hub on every push and publishes the image to GHCR on `v*` tags
- [x] `g2labs/hub/availability` is documented in `docs/mqtt.md` and has a schema in `contract/`
- [x] CLAUDE.md describes `service/` and its commands

## Comments

### 2026-11-04 — Claude

Done. `service/` holds the Hub: a TypeScript service on Node.js 24 (Fastify, `mqtt`, `better-sqlite3`) and a React + Vite admin page that it builds into `dist/public` and serves. It is configured by `HUB_MQTT_URL`, `HUB_MQTT_USERNAME`, `HUB_MQTT_PASSWORD`, `HUB_ADMIN_PASSWORD` and `HUB_DATA_DIR` (plus `HUB_HTTP_PORT`, `HUB_HTTP_HOST` and `HUB_MQTT_RECONNECT_S`). It publishes retained `online` (QoS 1) on `g2labs/hub/availability` with an `offline` last will, publishes `offline` itself on SIGTERM and reconnects after a lost connection. The admin page logs in with the admin password, keeps an HttpOnly session cookie (in memory, so a restart logs out) and shows the Broker connection. SQLite runs in WAL mode in `$HUB_DATA_DIR/hub.db`, with append-only migrations tracked in `user_version`. The first migration adds a `settings` table.

The vitest suite drives the Hub only through MQTT (an in-process aedes Broker, which can drop the Hub's socket to fire its will or restart to test reconnecting) and HTTP. The contract only gets the topic index entry and examples for `g2labs/hub/availability` here; checking the Hub's publishes against the contract is #32.

better-sqlite3 ships prebuilt modules for linux x64 and arm64, so its install script is denied and the image's build stage runs natively. The runtime stage only copies files, so no compiling happens under QEMU. CI's `hub` job runs the build and tests, then builds both images and loads SQLite in each. `hub-image` pushes to GHCR on `v*` tags.

I checked the amd64 image by hand against a real Mosquitto: it published `online`, served the page, logged in, reported the Broker as connected, created a WAL database in `/data`, and published `offline` on `docker stop`. The arm64 image builds locally, but there is no arm64 emulation on this host, so its first run will be in CI.
