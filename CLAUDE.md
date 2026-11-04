# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

g2labs-node is a Zephyr RTOS-based IoT node application (MIT licensed). As of the initial commit the repository contains no source code or build files yet — only `README.md`, `LICENSE` and a generic C `.gitignore`.

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

```bash
west build -b esp32c6_devkitc/esp32c6/hpcore .     # build for ESP32-C6
west build -b esp32s3_devkitc/esp32s3/procpu .     # build for ESP32-S3
west build -p always -b <board> .                  # pristine rebuild (needed when switching boards)
west flash
west espressif monitor                             # serial console
west build -t menuconfig                           # interactive Kconfig
```

Use separate build dirs (`-d build-c6`, `-d build-s3`) to keep both targets built side by side.

Once the app exists, it is expected to follow the usual Zephyr application layout (`CMakeLists.txt`, `prj.conf`, `src/`, `boards/` overlays and optional `Kconfig`).

Note: `.gitignore` does not currently exclude the `build/` directory created by `west build`.

## Agent skills

### Issue tracker

Issues are tracked in GitHub Issues for `grzegorz-grzeda/g2labs-node` via the `gh` CLI. See `docs/agents/issue-tracker.md`.

### Triage labels

Default label vocabulary: `needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, `wontfix`. See `docs/agents/triage-labels.md`.

### Domain docs

Single-context: one `GLOSSARY.md` and `docs/adr/` at the repo root. See `docs/agents/domain.md`.
