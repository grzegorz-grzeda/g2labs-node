---
number: 3
title: "CI: run core tests and build both boards on every push"
status: closed
closed_reason: completed
labels: [ready-for-agent]
parent: 1
blocked_by: [2]
created: 2026-11-04
closed: 2026-11-04
---

# CI: run core tests and build both boards on every push

## What to build

Every push and pull request runs the Node core tests on `native_sim` under twister and builds both board targets with MCUboot, so a broken build or failing behaviour is caught before merge.

## Acceptance criteria

- [ ] A GitHub Actions workflow runs on push and pull request
- [ ] It runs the `native_sim` twister tests and fails the run on any test failure
- [ ] It builds the ESP32-C6 and ESP32-S3 targets with sysbuild and MCUboot, including fetching the Espressif blobs
- [ ] The workflow passes on main

## Comments

### 2026-11-04 — grzegorz-grzeda

Implemented in 26caee7, with fixes in 862c9c4 (cache key) and 85b2345 (module Python requirements). `.github/workflows/ci.yml` runs on every push and pull request: it recreates a minimal west workspace pinned to Zephyr decee24b4fb (hal_espressif, mcuboot, mbedtls, tf-psa-crypto), fetches the Espressif blobs, installs Zephyr SDK 1.0.1 (RISC-V + ESP32-S3 Xtensa toolchains), runs the Node core tests on native_sim under twister, and builds both board targets with sysbuild + MCUboot. Passing on main: https://github.com/grzegorz-grzeda/g2labs-node/actions/runs/41754501812
