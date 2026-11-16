# The TrimUI Smart Pro's kit 2b Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A test kit for the TrimUI Smart Pro (TG5040), `v1.0.6-kit.2b`: its own image (`ZyperOS-v1.0.6-kit.2b-smartpro.img.xz`) that boots to the home, takes the player's own closed files as the Brick's does, reads its buttons and sticks, and shows the 4:3 launcher on its 16:9 panel with side bars; kit 2's test list plus the sticks and the picture.

**Architecture:** The Smart Pro is the Brick's plastron board with its own first boot stage (SPL: its memory differs), device tree and panel, and a `smartpro` profile. Unlike the Brick, its controls are not pins: both halves of the body are small controllers on two serial ports, so a small daemon of ours turns their frames into one evdev gamepad (uinput, already in the Brick's 4.9 kernel), as the launcher, Diatom and ports read today. The 1024x768 canvas is drawn as it is and scaled to 960x720 in the middle of the panel; wide layouts are later design-system work.

**Spec:** `docs/superpowers/specs/2026-11-14-device-modular-design.md` (section 6c; section 7, closed files; the appendix "Measurements"). Draft: `smartpro-kit-2b-plan.md` in the coordinator's notes folder.

## Global Constraints

- Kit 2 is not delayed and the Brick and Pixel 2 do not change: `make pixel-gate BASE=origin/main GATE='--lang en'` 0 frames changed; `--device brick` shots the same.
- Closed files are the player's own copy, fetched by the installer (section 7), never in the image, the update pack or the kit. TrimUI's firmware is read statically on this Mac only: never run, never shipped, nothing copied from it but facts (memory values, pins, the panel's init words, register values).
- Original art only: the handheld from TrimUI's published outline and views (reference only, never traced), no TrimUI mark.
- Nothing on the Smart Pro's own storage (eMMC) is written. The kit boots from the card.
- No tester request and no publish without Tarik's own OK. Downloads need his OK (file, source, size).
- Public text in Tarik's first person; no AI mention.

## Known (2026-11-16, no device)

- TrimUI's public repos: `trimui/firmware_smartpro` (release v1.1.1, 2025-11-28: `trimui_tg5040_20251128_v1.1.1.zip`, 251,273,751 bytes; `sd_recovery_tg5040_smart_pro_v1.1.1_20251128.zip`, 2,016,789,401 bytes), `trimui/toolchain_sdk_smartpro` (`SDK_usr_tg5040_a133p.tgz`, 185,049,621 bytes), `trimui/assets_smartpro`. Stock moved its GPU files from DDK 1.11 to 1.19 at 1.0.4; ours stay the Sonic Pad's 1.11 for plastron's 4.9 GPU driver.
- Controls (from `Jpe230/Trimui_Inputd`, GPL-3.0 as ZyperOS): two halves on `/dev/ttyS4` (left) and `/dev/ttyS3` (right), 19200 baud 8N1 raw, powered by GPIO 110 and 114 driven high. Each sends 8-byte frames: `0xFF`, a byte not read, the buttons, the stick's X (hi, lo), Y (hi, lo), `0xFE`. Left buttons: 0x01 L1, 0x02 L2, 0x04 up, 0x08 left, 0x10 right, 0x20 down, 0x80 MENU. Right: 0x01 R1, 0x02 R2, 0x04 Y, 0x08 X, 0x10 B, 0x20 A, 0x40 SELECT, 0x80 START. Both sticks' axes are inverted. GPIO 243 is a switch (reported as SW_TABLET_MODE), GPIO 227 the rumble motor. Stock keeps each unit's stick calibration in `/mnt/UDISK/joypad.config` and `joypad_right.config` (x_min, x_max, y_min, y_max, x_zero, y_zero, deadzone), on the eMMC.
- Volume and POWER are not on the halves (pins and the PMIC, to be read in Task 1).

## Review Focus

1. A frame split across two reads, or junk before `0xFF`: the reader resyncs on the next `0xFF ... 0xFE` and loses no press (Task 3's check).
2. A half that sends nothing (unplugged ribbon, not powered): the other half and the d-pad still work, and the log says which half is silent (Task 3).
3. A stick at rest that is not centred: no drift on the home (learned centre plus a dead zone) (Task 4).
4. The side bars: every screen, the tutorial and the keyboard keep their touch-free layout inside 960x720 with nothing cut (Task 2's sheet).
5. A Brick card put in a Smart Pro, or the other way: the SPL refuses or nothing happens; never a write (Task 6, the image's boot check).

## Tasks

0. **Tarik's OK to download** `trimui_tg5040_20251128_v1.1.1.zip` (251 MB, TrimUI's GitHub release) for a static read. Blocks 1, 5 and 6; Tasks 2, 3 and 4 do not wait.
1. **The static read** (about a day; notes in the coordinator's folder, facts into the spec's appendix): boot0's DRAM parameters (type, clock, timings, as the Brick's `0002-a133-lpddr3.patch` holds them), the device tree (decompiled: panel, PMIC, UARTs and their pins, GPIOs 110/114/227/243, Wi-Fi), the panel's driver and init words, the stock daemon's evdev layout (its button order, so SDL's mapping for ports matches stock as brick-pad's does), and the volume and POWER keys.
2. **The profile and the picture** (host first): `src/device/smartpro_profile.c` + `smartpro_buttons.h` (id `smartpro`, name "TrimUI Smart Pro", userdata `smartpro`, panel 720x1280 turned (the turn from Task 1, 90 until then), active 109.8 x 61.8 mm, aspect 16:9, brightness MENU+VOL as the Pixel 2's (no F1/F2), sticks as `DEV_CAP_STICK`), added to `src/profile.c`'s list and `tools/devices-check.c`; the host window at 1280x720 for a 16:9 profile with the canvas scaled to 960x720 and centred on black bars; a contact sheet of the home, MENU, the tutorial, the keyboard and the Controls tab under `--device smartpro`, both themes.
3. **The pads reader** (`tools/sppad.c`, test first): a pure part (`sp_feed(sp_reader *, const uint8_t *, size_t)` gives decoded frames: buttons and raw axes per half) checked by `tools/sppad-check.c` in `make check` (split frames, junk, resync, a silent half), and the device part (GPIOs 110/114 high, both ports at 19200 8N1, one uinput gamepad in stock's layout from Task 1, ABS_X/ABS_Y and ABS_RX/ABS_RY, the d-pad as a hat as brick-pad does). Built by the smartpro board, started by its init before the launcher.
4. **The sticks in use** (shared with the Brick Pro, step 8): the launcher takes the left stick as the d-pad (a threshold, the d-pad's repeat), Diatom passes both sticks to cores as analog; a learned centre and a dead zone (stock's eMMC calibration file only with Tarik's OK: reading it means mounting the eMMC read-only).
5. **Its handheld** (`src/device/smartpro_model.c`, `TD_ART_SMARTPRO`): horizontal, the shoulders on the top edge as the Pixel 2's, two sticks; from TrimUI's published views (Task 0's question also asks for those). Placeholder until then: the profile's `model` NULL.
6. **The board and the image**: `plastron/buildroot/board/smartpro/` from the Brick's (its SPL defconfig with Task 1's memory values, its device tree, the panel patch, `genimage.cfg`, label `SMARTBOOT`), `smartpro_defconfig`, `DEVICE=smartpro` through `mk/` and the release (`--kit smartpro`), the installer's device list, `res/update/min-system-smartpro`, its package list from `make third-party` on a release build.
7. **Kit 2b**: the kit branch (version `1.0.6-kit.2b`, notes = kit 2's list plus the sticks and the picture, in Tarik's voice), built and checked as kit 2 was; published only with Tarik's own OK.
