# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

g2labs-node is a Zephyr RTOS-based IoT node application (MIT licensed). The product spec is GitHub issue #1; domain terms are in `GLOSSARY.md`.

## Code layout

- `core/` — the Node core: all decision-making, plain C. It must not include Zephyr Wi-Fi, Bluetooth, MQTT or MCUboot headers. Inputs are calls into `core/include/node/core.h`; outputs go only through the ports in `struct node_ports` (MQTT publish, Wi-Fi requests, image confirm). `core/core.cmake` adds it to a Zephyr `app` target.
- `src/adapters/` — the only code that touches Zephyr radio, network and MCUboot APIs; each function implements one port.
- `src/main.c` wires adapters into the core; `src/shell.c` holds the `node …` UART shell commands.
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

## Test commands

Core tests run on `native_sim` under twister (needs host gcc with 32-bit support):

```bash
west twister -T tests -p native_sim
```

Build and twister output (`build*/`, `twister-out*/`) is git-ignored.

## Agent skills

### Issue tracker

Issues are tracked in GitHub Issues for `grzegorz-grzeda/g2labs-node` via the `gh` CLI. See `docs/agents/issue-tracker.md`.

### Triage labels

Default label vocabulary: `needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, `wontfix`. See `docs/agents/triage-labels.md`.

### Domain docs

Single-context: one `GLOSSARY.md` and `docs/adr/` at the repo root. See `docs/agents/domain.md`.
