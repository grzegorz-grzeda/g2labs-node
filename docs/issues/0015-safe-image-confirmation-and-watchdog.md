---
number: 15
title: "Safe image confirmation and watchdog"
status: closed
closed_reason: completed
closed: 2026-11-05
labels: [ready-for-agent]
parent: 1
blocked_by: [6, 14]
assignee: claude
created: 2026-11-04
---

# Safe image confirmation and watchdog

## What to build

An update can't strand a Node: a new image confirms itself only after its first successful Broker connection, and a hardware watchdog reboots a hung image so MCUboot reverts it.

## Acceptance criteria

- [x] A pending test image is confirmed only after its first successful Broker connection, never before
- [x] An image that never reaches the Broker reverts to the previous one on the next reboot
- [x] A hardware watchdog is fed while the Node is healthy; a hung image reboots and, if unconfirmed, reverts
- [x] Verified on hardware: an image with a broken Broker setting reverts and the Node is reachable over SMP again
- [x] Core tests on `native_sim` cover confirm-after-Broker and no confirm before

## Comments

### 2026-11-05 — claude

Done: the core confirms the running image on the first Broker connection of each boot (a failed confirm is logged and retried on the next connection), so an image that never gets there reverts on the next reboot. A hardware watchdog (`watchdog0`), started first thing in `main` and fed every turn of the main loop, resets a hung Node; `CONFIG_NODE_WATCHDOG_TIMEOUT_S` (default 30 s; the ESP32 driver resets after twice that) sets how long the loop may stall. Core tests in `tests/core/src/test_image_confirm.c`; `docs/management.md` updated. Built for both boards.

Verified on an ESP32-C6 over Wi-Fi, with SMP from `smpmgr`:
- A good test image booted, reached the Broker at 9.9 s and only then logged `image confirmed`; it stayed after the next reset.
- With the Broker port set wrong (1884), a test image retried the Broker (connection refused) for a minute and never confirmed, kept answering SMP echo and shell, and one `os reset` over SMP reverted it to the previous image, reachable over SMP again. The Broker setting was then put back.
- A test image patched to stop before its main loop was reset by the watchdog (`rst:0x7 (TG0_WDT_HPSYS)`) 60 s after boot, twice the 30 s timeout as the driver's two stages predict, and MCUboot reverted it.

The ESP32-S3's watchdog timing is left to the smoke test (#16).

An unconfirmed image that can't reach the Broker but doesn't hang is not rebooted by the Node itself: it reverts on the next reset over SMP, or on a power cycle if it broke Wi-Fi too.
