# Build, card and release by device (step 4) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The build, the card, System Update, the release, the installer and the dev tools take the device from data the profile states once, instead of testing for PowerVR or assuming the Pixel 2; a Pixel 2 payload built through the new path is byte for byte the payload the old path built.

**Architecture:** Each device's build facts (architecture, `-mcpu`, Diatom port, card layout, the payload's helpers) live in `src/device/<id>_build.c`, which the launcher never links, so its binary does not change. A host tool, `tools/devicegen.c`, writes `config/device/<id>.mk` and `config/device/<id>.system` from them; both are committed, as `src/i18n_tables.c` is, and `make check-devicegen` fails when they are stale. `mk/cross.mk`, `mk/payload.sh`, `mk/px2-sdk-build.sh` and plastron (one `BR2_PLASTRON_DEVICE` symbol per defconfig) read those files. Later PRs read the image's facts at run time, merge the card scripts, build the release over `config/devices`, and give the cores an architecture.

**Tech Stack:** C (gnu11), POSIX sh, GNU make, Buildroot 2026.02.3, Docker (plastron's builder and the SDK volume).

**Spec:** `docs/superpowers/specs/2026-11-14-device-modular-design.md` (section 5, section 9 step 4 as amended by step 3's calls).

## Global Constraints

- One PR each for 4a to 4e below, in that order, each from the `main` that has the one before.
- 4a: `make px2-payload` on `main` and on the branch give the same payload tree, every file's SHA-256 the same (the proof the spec asks for).
- Never run `mk/fetch-vendor.sh`; never take `/tmp/plastron-build-lock` by hand; never run `zyperos.elf` over SSH; any program test on the release image runs under `timeout`; device tests on the 32 GB test card (62333952 sectors) only, under `/tmp/px2-lock`, in the order the coordinator gives.
- The Brick's own card numbers are written in step 6; until then its build data states what its genimage and datapart have today.
- Public text in Tarik's first person, no AI mention, no Co-Authored-By.

## Calls this plan makes

- **Step 4 is five PRs, not one.** Merging the card scripts changes `launch.sh` and reading the image's facts changes the launcher and `zyperos-update`, so neither can keep the payload byte for byte; the build path alone can, and goes first (4a) with that proof.
- **The build facts are not in `device_profile`.** Fields there change the launcher's binary; `src/device/<id>_build.c` with its own `dev_build` type, linked only by the generator and the checks, keeps 4a's payload the same.
- **The generated files are committed** (`config/device/<id>.mk`, `<id>.system`), checked by `check-devicegen`, so a cross build, the SDK build and Buildroot need no host compiler step.
- **plastron's symbol is `BR2_PLASTRON_DEVICE`**, in plastron's own `Config.in`, not `BR2_PACKAGE_ZYPEROS_DEVICE`: it names the board's device for every package (splash, diatom, the squashfs order), with ZyperOS or without (the base flavor).
- **SDL2's PowerVR options keep their PowerVR test** (`external.mk`): they are about the GPU library, not the device.

## Review Focus

1. The Pixel 2 payload: same files, same bytes (4a's proof, Task 4).
2. A Buildroot build of the base flavor (no ZyperOS): the device symbol still says pixel2, splash and the squashfs order as before.
3. QEMU's build (`qemu_defconfig`): pixel2 as today, no Brick package.
4. A stale `config/device/*.mk` after a change to a `_build.c`: `check-devicegen` fails and names the file.
5. `/etc/zyperos/system` on the Pixel 2: the three lines it had, in the same order, then the new ones; readers that read only `version`, `update`, `device` unaffected.

---

## 4a: the build by device

### Task 1: The build data

**Files:**
- Create: `src/device/build.h` (`dev_build`, `dev_helper` bits), `src/device/pixel2_build.c`, `src/device/brick_build.c`
- Test: `tools/devices-check.c` (links them through `$(DEVICE_BUILDS)`, a new Makefile list)

**Interfaces:**
- `typedef enum { DEV_HELPER_PORTKEYS = 1, DEV_HELPER_ZYHOME = 2, DEV_HELPER_UPDATE = 4, DEV_HELPER_P2D = 8, DEV_HELPER_PORTS = 16, DEV_HELPER_BTPLAYER = 32, DEV_HELPER_BTALSA = 64 } dev_helper;`
- `typedef struct { const char *id, *arch, *mcpu, *cpu, *diatom_port, *table; int part_boot, part_root_a, part_root_b, part_games; unsigned helpers; } dev_build;`
- `extern const dev_build device_build_pixel2, device_build_brick;`
- Values: pixel2 = aarch64, cortex-a53 (the shared baseline, as `mk/cross.mk:14` today), cortex-a35, port pixel2, mbr, 1/2/4/3, PORTKEYS|ZYHOME|UPDATE|P2D|PORTS. brick = aarch64, cortex-a53, cortex-a53, port brick, gpt, 1/2/0/4 (datapart finds games by GPT name, p4 after state), BTPLAYER|BTALSA.

- [ ] Write the cases in `devices-check.c` (each id equals its profile's id; the values above), run `make check-devices` (FAIL), add the files, run (pass), commit.

### Task 2: The generator and its committed files

**Files:**
- Create: `tools/devicegen.c`, `config/device/pixel2.mk`, `config/device/pixel2.system`, `config/device/brick.mk`, `config/device/brick.system`
- Modify: `Makefile` (`device-files` writes them; `check-devicegen` writes them into `build-native/devicegen/` and `cmp`s; in `CHECKS`; a row in `docs/building.md`)

`<id>.mk` (make syntax, comment line first saying it is generated and from which file):
`ZYDEV_ID`, `ZYDEV_ARCH`, `ZYDEV_MCPU`, `ZYDEV_CPU`, `ZYDEV_DIATOM_PORT`, `ZYDEV_HELPERS` (space-separated words: portkeys zyhome update p2d ports btplayer btalsa, in that order).
`<id>.system` (lines for `/etc/zyperos/system`, after version/update/device): `table mbr`, `part-boot 1`, `part-root-a 2`, `part-root-b 4` (only when not 0), `part-games 3`.

- [ ] Write `check-devicegen` first (regenerates, compares), run it (FAIL, no tool), write the tool and the files with `make device-files`, run (pass), change a value in a scratch copy to see it fail with the file's name, commit.

### Task 3: The build reads them

**Files:**
- Modify: `mk/cross.mk` (`include config/device/$(DEVICE).mk`; `-mcpu=$(ZYDEV_MCPU)` where `cortex-a53` was written), `mk/payload.sh` (the helpers from `ZYDEV_HELPERS` read out of `config/device/$DEVICE.mk`, the same files laid out the same way), `mk/px2-sdk-build.sh` (its targets from the helpers)
- plastron: `buildroot/Config.in` (`config BR2_PLASTRON_DEVICE string`, default "pixel2"), `configs/brick_defconfig` (`BR2_PLASTRON_DEVICE="brick"`), `package/zyperos/zyperos.mk` (device from the symbol; `ZYPEROS_EXTRA` and the payload call from the helpers; one install branch; `/etc/zyperos/system` on every device: version, update (1 where the helpers have update), device, then `<id>.system`, then `has` with what the image has: usbdrive, hostapd), `package/diatom/diatom.mk` and `Config.in` (port from the device's file), `package/splash/splash.mk` and `Config.in` (fb splash for brick), `external.mk` (squashfs order by device)

- [ ] Run `make check` and `make check-device-compile` (Ubuntu); build plastron's Pixel 2 image and check `/etc/zyperos/system` and the payload; commit.

### Task 4: The proof

- [ ] `make px2-payload` on a checkout of `main` and on the branch; `find build/px2/payload -type f -exec shasum -a 256 {} +` sorted for both; `diff` empty. Also the plastron image's `/usr/share/zyperos/card` against main's, by the same listing.
- [ ] Record in the spec (step 4's calls), open the PR, tell the coordinator.

## 4b: the image's facts, read at run time

`update/slots.c` takes the boot and root partitions from the system file's `part-*` lines (today's 1/2/4 when absent, so an image from before still works); `update/main.c` and `update/apply.sh` take the device from its `device` line where `--device` is not given; `plat_system_has` reads the `has` line (its `access()` checks stay only for a system file with no `has` line); `tools/px2`, `px2-deploy.sh`, `px2-push.sh` take `PX2_DEVICE` (address, key, userdata folder, lock), `px2-shot`/`px2-lat` get an fbdev path for the Brick. Device-tested on the test card (a System Update check against a local list, as `tools/update-check.sh` does).

## 4c: one card script

`sd/zyperos/launch.sh` from today's `sd/pixel2/launch.sh` (the fuller one), with `sd/<id>/device.sh` for what differs (environment, userdata folder, SDL drivers, the backlight helper, radios, Bluetooth, the loading still, ports); `mk/payload.sh` lays out both. The Brick gains ports, `run:`, reboot and restart flags and the loading still where its device file can do them, and keeps Bluetooth. `tools/launch-start-check.sh` runs both devices. Device-tested on the Pixel 2; the Brick by its testers (step 6).

## 4d: the release over a list of devices

`config/devices` (`pixel2`); `tools/release.sh` loops over it (image, sources, update lists, SHA256SUMS, BUILT-FROM) and refuses to publish without a `Tested on <id>` line per device in `docs/releases/v<ver>.md`; `res/update/min-system` per device (`res/update/min-system-<id>`, the Pixel 2's `1.0.3`); CI's release check per device; the installer lists the devices with an image in the newest release (a device without one is not offered); the site's download page and `downloads.js` one button per released device.

## 4e: the cores' architecture

`vendor/<arch>/cores` (`ZYDEV_ARCH`), with a one-time move of an existing `vendor/cores` by `mk/payload.sh` and `make vendor` (never by running `mk/fetch-vendor.sh` here); every `mk/build-*.sh` writes there; the cores release keeps its asset names until its next tag; the payload drops the cores of systems with a `needs|TAG|gpu-frames` line on a device without GPU frames (its profile's `gpu_frames`, carried into `<id>.mk` as `ZYDEV_GPU_FRAMES`).
