# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

g2labs-node is a Zephyr RTOS-based IoT node application (MIT licensed). The product spec is GitHub issue #1; domain terms are in `GLOSSARY.md`.

## Code layout

- `core/` — the Node core: all decision-making, plain C. It must not include Zephyr Wi-Fi, Bluetooth, MQTT or MCUboot headers. Inputs are calls into `core/include/node/core.h`; outputs go only through the ports in `struct node_ports` (MQTT connect/disconnect/publish, Wi-Fi requests, image confirm), which also answer the few platform queries the core makes (link RSSI, free memory). `core/core.cmake` adds it to a Zephyr `app` target.
- `src/adapters/` — the only code that touches Zephyr radio, network, MQTT, MCUboot and settings APIs. The MQTT adapter runs its own thread for the blocking socket work, so its ports never block the core. Port functions implement `struct node_ports`; the rest are setup (`wifi_adapter_init`), persistence (`settings_adapter_*`, which load into and save from the core's public API), the Wi-Fi MAC for the Node ID (`wifi_adapter_mac`) and diagnostics (`wifi_adapter_link_info`) used by `src/`.
- `src/main.c` wires adapters into the core and runs it: adapters post events with `app_post_event()` (any thread) and the main loop feeds them to the core, telling it the time since boot (`node_core_tick()`) at least once a second for its Wi-Fi and Broker timeouts. The core isn't thread-safe, so anything else touching it (the shell) holds `app_core_lock()`.
- `src/shell.c` holds the `node …` UART shell commands (`node version`, `node id`, `node name set|clear`, `node network add|remove|list|status`, `node broker set|login|logout|clear|show`).
- `docs/mqtt.md` — the Node's MQTT topics and JSON fields, a contract with the Aggregator and Home Assistant; update it with any change to them.
- `tests/core/` — ztest suite driving the core through fake ports (`src/fake_ports.c`) on `native_sim`. Tests assert only on recorded outputs, never on core internals.
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

## CI

`.github/workflows/ci.yml` runs the core tests and both sysbuild builds on every push and pull request. It recreates a minimal workspace pinned to `ZEPHYR_REVISION` (with only the west modules in `WEST_PROJECT_FILTER`) and Zephyr SDK `ZEPHYR_SDK_VERSION`. When the local workspace's Zephyr or SDK is updated, bump those values in the same change.

## Agent skills

### Issue tracker

Issues are tracked in GitHub Issues for `grzegorz-grzeda/g2labs-node` via the `gh` CLI. See `docs/agents/issue-tracker.md`.

### Triage labels

Default label vocabulary: `needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, `wontfix`. See `docs/agents/triage-labels.md`.

### Domain docs

Single-context: one `GLOSSARY.md` and `docs/adr/` at the repo root. See `docs/agents/domain.md`.
