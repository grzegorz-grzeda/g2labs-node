# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

g2labs-node is a Zephyr RTOS-based IoT node application (MIT licensed), plus the Hub that manages the Nodes (`service/`). The product specs are #1 (the Node firmware) and #17 (the Hub) in `docs/issues/`; domain terms are in `GLOSSARY.md`.

## Code layout

- `core/` — the Node core: all decision-making, plain C. It must not include Zephyr Wi-Fi, Bluetooth, MQTT or MCUboot headers. Inputs are calls into `core/include/node/core.h`; outputs go only through the ports in `struct node_ports` (MQTT connect/disconnect/publish, Wi-Fi requests, image confirm), which also answer the few platform queries the core makes (link RSSI, free memory). `core/core.cmake` adds it to a Zephyr `app` target.
- `src/adapters/` — the only code that touches Zephyr radio, network, MQTT, MCUboot and settings APIs. The MQTT adapter runs its own thread for the blocking socket work, so its ports never block the core. Port functions implement `struct node_ports`; the rest are setup (`wifi_adapter_init`), persistence (`settings_adapter_*`, which load into and save from the core's public API), the Wi-Fi MAC for the Node ID (`wifi_adapter_mac`) and diagnostics (`wifi_adapter_link_info`) used by `src/`.
- `src/main.c` wires adapters into the core and runs it: adapters post events with `app_post_event()` (any thread) and the main loop feeds them to the core, telling it the time since boot (`node_core_tick()`) at least once a second for its Wi-Fi and Broker timeouts. The core isn't thread-safe, so anything else touching it (the shell) holds `app_core_lock()`.
- `src/shell.c` holds the `node …` UART shell commands (`node version`, `node id`, `node name set|clear`, `node network add|remove|list|status`, `node broker set|login|logout|clear|show`).
- `Kconfig` — the app's own options (`CONFIG_NODE_BROKER_FAILOVER_S`, how long the Broker may be unreachable before the Node moves to another Known Network), passed to the core through `struct node_info`.
- `contract/` — the MQTT contract between Nodes and the Hub: `topics.json` (every topic pattern with publisher, retained flag and encoding), strict JSON Schemas 2020-12 in `schemas/` (core keywords only, no additional properties) and example payloads in `examples/`. It only grows (ADR-0005): add topics and optional fields, never remove or retype. `contract/check.py` checks the contract (examples, docs coverage) and, given the twister output, every publish the core tests made (the fake ports dump them to `contract_publishes.jsonl`): `contract/check.py --publisher node twister-out`. Its own tests: `python -m unittest discover -s contract`.
- `docs/mqtt.md` — the human explanation of the MQTT contract; update it together with `contract/` for any change to topics or payloads.
- `tests/core/` — ztest suite driving the core through fake ports (`src/fake_ports.c`) on `native_sim`. Tests assert only on recorded outputs, never on core internals.
- `service/` — the Hub (package `g2labs-node-service`), TypeScript on Node.js 24 (`service/.nvmrc`), separate from the Zephyr build. `src/hub.ts` wires `db.ts` (SQLite via `better-sqlite3`, WAL mode, append-only migrations in `PRAGMA user_version`), `broker.ts` (MQTT client: retained `g2labs/hub/availability` with an `offline` last will, reconnects) and `http.ts` (Fastify: the admin page from `dist/public` plus `/api`, behind one admin password and an in-memory session cookie); `src/main.ts` runs it from env vars (`src/config.ts`: `HUB_MQTT_URL`, `HUB_MQTT_USERNAME`, `HUB_MQTT_PASSWORD`, `HUB_ADMIN_PASSWORD`, `HUB_DATA_DIR`, `HUB_HTTP_PORT`, …). `web/` is the React + Vite admin page. `test/` drives the Hub only through MQTT (an in-process aedes Broker) and HTTP. `Dockerfile` builds the amd64/arm64 image; `docker-compose.example.yml` runs it with `network_mode: host` next to Mosquitto.
- `VERSION` — firmware version (`APP_VERSION_*` in `<zephyr/app_version.h>`, also used to sign the MCUboot image).

## Workspace layout

This repo is an out-of-tree Zephyr application living inside a west workspace, not a standalone project:

- Workspace root: `~/zephyrproject` (contains `zephyr/`, `modules/`, `bootloader/`, `tools/`)
- West manifest: `zephyr/west.yml` (the workspace uses the upstream Zephyr manifest; this app is not a west manifest repo)
- This app: `~/zephyrproject/apps/g2labs-node`

Zephyr builds rely on the workspace environment (`ZEPHYR_BASE` pointing at `~/zephyrproject/zephyr`, the Zephyr SDK, and the workspace's Python venv if one is used).

## Target hardware

The app targets two Espressif SoCs, so code must stay portable across both a RISC-V and an Xtensa chip:

| SoC | Zephyr board target | Core |
|---|---|---|
| ESP32-C6 | `esp32c6_devkitc/esp32c6/hpcore` | RISC-V (HP core; an `lpcore` target also exists) |
| ESP32-S3 | `esp32s3_devkitc/esp32s3/procpu` | Xtensa LX7 dual-core (PRO CPU; `appcpu` is the second core) |

Board-specific differences (pin mapping, peripherals, radio features: the C6 has 802.15.4/Thread/Zigbee and Wi-Fi 6, the S3 has neither) belong in `boards/<board>.overlay` / `boards/<board>.conf`, not `#ifdef`s in shared code where avoidable.

## Build commands

Run from the app directory. The Espressif HAL needs RF binary blobs fetched once per workspace:

```bash
west blobs fetch hal_espressif
```

The firmware is always built through sysbuild, which also builds MCUboot and signs the image with MCUboot's development key (the "default MCUboot signing key" warning is expected). `--sysbuild` is required because this workspace doesn't enable it by default.

```bash
west build --sysbuild -d build-c6 -b esp32c6_devkitc/esp32c6/hpcore .     # build for ESP32-C6
west build --sysbuild -d build-s3 -b esp32s3_devkitc/esp32s3/procpu .     # build for ESP32-S3
west build --sysbuild -p always -d build-c6 -b esp32c6_devkitc/esp32c6/hpcore .  # pristine rebuild
west flash -d build-c6                                                    # flashes MCUboot + Node image
python -m serial.tools.miniterm /dev/ttyACM0 115200                       # serial console (shell: `node version`)
west build -d build-c6 -t menuconfig                                      # interactive Kconfig (Node image)
```

`west espressif monitor` also works, but only for the default `build/` dir (it has no `-d` option).

`scripts/g2labs-node.py` wraps all of this. Run it from the activated venv (`~/zephyrproject/.venv`). Its subcommands are `build`, `flash` (build, flash, then monitor) and `monitor`. It also mirrors the Node's `node …` shell commands (`version`, `id`, `name`, `network`, `broker`) and adds `show`, a summary of them all. `provision` applies a Provisioning file, which is TOML; `provision --help` shows the format. `--port` defaults to the only `/dev/ttyACM*`/`/dev/ttyUSB*`, else `/dev/ttyACM0`. Opening the port leaves DTR/RTS as they are, because changing them resets the Node. Provisioning files hold plaintext passwords; `*.provision.toml` is git-ignored.

```bash
scripts/g2labs-node.py flash --board c6                # build, flash, open the shell (quit: Ctrl-])
scripts/g2labs-node.py show                            # version, Node ID, Known Networks, Broker
scripts/g2labs-node.py network add home                # prompts for the password
scripts/g2labs-node.py provision kitchen.provision.toml --dry-run
```

## Test commands

Core tests run on `native_sim` under twister (needs host gcc with 32-bit support):

```bash
west twister -T tests -p native_sim
```

Build and twister output (`build*/`, `twister-out*/`) is git-ignored.

## Hub commands

Run from `service/` with Node.js 24 (`nvm use`). better-sqlite3 ships prebuilt binaries for linux/macOS/Windows x64 and arm64, so its compile-from-source install script is denied in `allowScripts` in `package.json`.

```bash
npm ci                     # install
npm run build              # server into dist/server, admin page into dist/public
npm test                   # typecheck, then vitest against an in-process Broker
HUB_ADMIN_PASSWORD=x HUB_DATA_DIR=./data npm start   # run the built Hub (Broker on mqtt://localhost:1883)
npm run dev                # admin page with hot reload, /api proxied to a Hub on :8080
docker build -t g2labs-node-service .                # the container image
```

## CI

`.github/workflows/ci.yml` runs the core tests and both sysbuild builds on every push and pull request. It recreates a minimal workspace pinned to `ZEPHYR_REVISION` (with only the west modules in `WEST_PROJECT_FILTER`) and Zephyr SDK `ZEPHYR_SDK_VERSION`. When the local workspace's Zephyr or SDK is updated, bump those values in the same change.

In `ci.yml`, the `hub` job builds and tests `service/` and builds its image for amd64 and arm64 (under QEMU, loading better-sqlite3 in each). On a `v*` tag, `hub-image` pushes the multi-arch image to `ghcr.io/<owner>/g2labs-node-service` (a Release Candidate `vX.Y.Z-rcN` as its version only, a stable Release as its version and `latest`).

`.github/workflows/human-gate.yml` sets the `human-gate` status on every PR: it fails while the PR touches a path in `.github/gated-paths` or carries `ready-for-human`, until the maintainer adds `human-approved` (logic and tests in `.github/scripts/`).

## Agent skills

### Development flow

Every change goes through a branch (`<kind>/<issue>-<slug>`) and a squash-merged PR that auto-merges once CI, `claude-review` and `human-gate` pass. Merging never delivers: only a stable Release, approved by the maintainer through a release PR, reaches the fleet (ADR-0006). See `docs/agents/development-flow.md`.

### Review

The `review-pr` project skill (`.claude/skills/review-pr/`) reviews a PR with fresh context and records the `claude-review` check. Invoke it from a fresh subagent or session, never from the session that wrote the PR (see `docs/agents/development-flow.md`).

### Issue tracker

Issues are Markdown files in `docs/issues/`, one per issue, with status, labels, parent and blockers in front matter. `#N` means `docs/issues/NNNN-*.md`. See `docs/agents/issue-tracker.md`.

### Triage labels

Default label vocabulary: `needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, `wontfix`. See `docs/agents/triage-labels.md`.

### Domain docs

Single-context: one `GLOSSARY.md` and `docs/adr/` at the repo root. See `docs/agents/domain.md`.
