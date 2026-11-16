# The Brick's Back End and Kit 2 (Step 6) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A TrimUI Brick card image that a tester installs with the ZyperOS Installer and runs through kit 2's list: A/B system slots and System Update, the closed GPU and Wi-Fi files as the player's own copy, the device code caught up, and a log pack that reports the hardware.

**Architecture:** Five PRs. 6a gives the Brick the Pixel 2's slot scheme, sharing the bootstate script and keeping root B at partition 4. 6b takes the closed files out of the image: the installer fetches them into `closed/` on the boot partition and the image links to them. 6c and 6d are launcher work (device code, hardware report) that can run beside 6a and 6b. 6e builds kit 2.

**Tech Stack:** C (launcher, splash, rootswitch), U-Boot hush scripts, Buildroot (plastron), Rust/Tauri (installer), Python (checks, frame generator).

**Spec:** `docs/superpowers/specs/2026-11-14-device-modular-design.md` (sections 5, 6, 7, 8, 9 and the map). Kit 2's list: `brick-kit-2-test-list.md` in the coordinator's notes folder.

## Global Constraints

- "ZyperOS's Brick and Brick Pro images, System Update packs and test kits carry none of the closed files."
- The installer "downloads them from the Sonic Pad SDK at its pinned commit, checks each against plastron's `files.sha256`, and writes them to the card's folder for them, outside the system slots ... it never sends them anywhere else."
- "System Update never touches the folder."
- "A card without the files says so on its first screen, in words a player can act on ('Run the ZyperOS Installer's Update on a computer'), rather than a black screen."
- "If the download fails, the installer says why and installs nothing."
- The Brick's own storage (eMMC) is never touched: its device tree has no node for it.
- The Pixel 2 does not change: its `boot.scr`, payload and image are byte-for-byte the same, and its pixel gate shows 0 changed frames.
- Original art only, no TrimUI marks. Public text (commits, PRs, docs) in Tarik's first person, no AI mention, no Co-Authored-By.
- Never run `mk/fetch-vendor.sh`; never take `/tmp/plastron-build-lock` by hand; ask the coordinator before any release or published kit.

## Decisions this plan makes

- **The Brick's card, GPT:** p1 `BRICKBOOT` (FAT, 64 MiB, folders `a/`, `b/`, `closed/`, `bootstate.txt`), p2 root A (`plastron-root`, 512 MiB squashfs), p3 state (`plastron-state`, 16 MiB ext4), p4 root B (`plastron-root-b`, 512 MiB, empty), p5 games (`ZYPEROS`, exFAT, made by `datapart` at the first start after p4). Root B at p4 matches the Pixel 2, so the shared bootstate script's `zy_part` (2 or 4) holds for both. No ZyperOS Brick card exists yet, so there is no old layout to carry.
- **Boot script:** `board/zyperos/boot-slots.cmd` splits into `bootstate.cmd` (its lines up to `zy_part`) and `boot-slots.cmd` (the Pixel's load and boot), so the Pixel's `boot.scr` is the same text. The Brick's `boot.scr` is `board/brick/boot-prelude.cmd` (the PLL pokes, `devtype mmc`, `devnum 0`, `distro_bootpart 1`, `zy_charge 0`) + `bootstate.cmd` + `board/brick/boot-slot.cmd` (loads `/${slot}/Image.lz4`, unlz4, `/${slot}/initramfs.cpio.gz`, `/${slot}/brick.dtb`, the fdt fixups, and `zyperos.root=/dev/mmcblk0p${zy_part} zyperos.slot=${slot} zyperos.trial=${trial} ${zy_theme}`).
- **U-Boot's own fallback** boots slot A's files (`/a/Image`, `/a/initramfs.cpio.gz`, `/a/brick.dtb`): a new U-Boot patch, and the committed boot loader rebuilt.
- **The closed files' folder:** `closed/` on p1, the 13 files of the two `files.sha256` under their file names. p1 is FAT, which the installer can write without mounting; the games partition does not exist until the first start. The image ships symbolic links at the files' usual places (`/usr/lib/...`, `/lib/firmware/...`) pointing into `/boot/closed/`, and mounts p1 at `/boot` before the GPU driver loads.
- **The installer** carries the list (name, path at `CrealityTech/sonic_pad_os@7f37a7f`, sha256) generated at build time from plastron's two `files.sha256`, fetches from `raw.githubusercontent.com`, keeps them in its cache, and writes them into p1 with the `fatfs` crate over the raw device after the image. It fetches before it writes anything.
- **Kit 2 changes:** step 11 says the Brick stays off on the charger (U-Boot patch 0009, by design), not that a charging screen shows. Bluetooth, ports, PortMaster, zyhome (and its MENU code from the profile), portkeys, the boot theme, saved stills, side lights, GPU-frame cores and Diatom's Brick stubs (On-Screen Stats, the DS's second screen, rumble, power steps) stay out of kit 2: each is its own later task, and none is on kit 2's list.
- **How a tester gets kit 2:** the kit's page has the image, its SHA256SUMS and installer builds made from the same commit; the installer takes the image from a file (Task 6b.5) and fetches the closed files as for a release.

## Review Focus

- A card whose `closed/` is incomplete (a copy cut short, a file of the wrong size): the Brick shows the missing-files screen, never a black screen, and the installer's Update repairs it. Tests: Task 6b.3 (boot check) and Task 6b.4 (installer repairs a partial folder).
- The download fails part way (offline, GitHub's rate limit, a wrong checksum): the installer writes nothing to the card and says which file and why; a second try uses what the cache holds. Test: Task 6b.4.
- `datapart` on a card that already has its games partition (a second start, a reinstalled card): it never makes or formats a partition that exists. Test: Task 6a.3.
- System Update on the Brick when p1 is short of room for a slot's files: it stops before writing and says so. Test: Task 6a.5.
- The Pixel 2's boot after the boot script split: `boot.scr` byte-for-byte the same. Test: Task 6a.2.

---

## PR 6a: The Brick's card with A/B slots

### Task 6a.1: The layout as data

**Files:**
- Modify: `src/device/brick_build.c` (`.part_root_b = 4`, `.part_games = 5`, helpers gain `update`)
- Modify: `config/device/brick.mk`, `config/device/brick.system` (regenerated by `tools/devicegen`)
- Modify: `update/slots.c:127` (no fallback to 4 when the system file names no root B: no slots then)
- Test: `tools/devices-check.c`, `tools/update-check.sh`

**Interfaces:**
- Produces: `brick.system` lines `part-boot 1`, `part-root-a 2`, `part-root-b 4`, `part-games 5`, `has ... update`.

- [ ] **Step 1: Failing checks.** `devices-check.c`: `builds()` asserts the Brick's `part_root_b == 4` and `part_games == 5` and that `update` is among its helpers. `update-check.sh`: a case with a system file that has no `part-root-b` line, where `slots_read` returns false (no slots), and a Brick case where slot B is `p4`.
- [ ] **Step 2:** Run `make check-devices check-devicegen check-update`; expect the new cases to fail.
- [ ] **Step 3:** Change `brick_build.c` and `slots.c`; run `make devicegen` to regenerate the two files.
- [ ] **Step 4:** Run the same checks; expect them to pass.
- [ ] **Step 5:** Commit.

### Task 6a.2: The boot script, shared

**Files:**
- Create: `plastron/buildroot/board/zyperos/bootstate.cmd` (the slot choice and `theme.txt`, up to and with the `zy_part` line)
- Modify: `plastron/buildroot/board/zyperos/boot-slots.cmd` (keeps the Pixel's load and boot)
- Modify: `plastron/buildroot/board/zyperos/post-image-slots.sh` (prelude + `bootstate.cmd` + the board's slot tail; the tail's file named by the board, `boot-slots.cmd` by default)
- Create: `plastron/buildroot/board/brick/boot-prelude.cmd`, `plastron/buildroot/board/brick/boot-slot.cmd`
- Delete: `plastron/buildroot/board/brick/boot.cmd`

- [ ] **Step 1:** Before the change, build the Pixel's `boot.cmd` text (`cat board/px2/boot-prelude.cmd board/zyperos/boot-slots.cmd`) and keep its sha256.
- [ ] **Step 2:** Split the file and change the assembly. Check: the Pixel's assembled `boot.cmd` has the same sha256.
- [ ] **Step 3:** Write the Brick's prelude and tail as the Decisions say. Keep `boot.cmd`'s PLL pokes, `pllcpux`, the fdt fixups, `cma=64M`, `rdinit=/rdinit`, `loglevel=1` and `bootscr=1`. Drop `root-update` (rootswitch's development path moves to slot A, Task 6a.4).
- [ ] **Step 4:** A post-image check in `board/brick/post-image.sh`: every `/${slot}/<file>` the tail loads is in `boot.vfat`'s `a/` (read with `mdir`), and the build fails when one is missing.
- [ ] **Step 5:** Commit.

### Task 6a.3: The image: slots, state, games

**Files:**
- Modify: `plastron/buildroot/board/brick/genimage.cfg` (the Decisions' layout; `boot.vfat` holds `a/` and `bootstate.txt`)
- Modify: `plastron/buildroot/board/brick/post-image.sh` (the slot size check as `post-image-slots.sh` does it, `bootstate.txt`)
- Modify: `plastron/buildroot/board/brick/rootfs-overlay/etc/init.d/datapart` (the games partition as p5, after the last partition; only when no p5 exists)
- Modify: `plastron/buildroot/board/brick/rootfs-overlay/etc/fstab` or the init script that mounts (p1 at `/boot`, vfat, before `init.d/brick`)

- [ ] **Step 1:** Build the image (`make brick` in plastron, under the build lock as the scripts take it). Check with `sgdisk -p` on the `.img`: five entries as the Decisions say once `datapart`'s step is simulated, four in the image, names and sizes as listed.
- [ ] **Step 2:** A host test for `datapart`'s choice (a shell function that reads `sfdisk --json`, factored so a test feeds it fixtures): it makes p5 on a four-partition card, does nothing on a card with p5, and refuses (logs, no change) on a card whose p4 is not `plastron-root-b`.
- [ ] **Step 3:** Commit.

### Task 6a.4: rootswitch by slot

**Files:**
- Modify: `plastron/buildroot/package/brick-rootswitch/src/rootswitch.c`
- Test: a host build of `rootswitch.c`'s cmdline parser under `plastron/scripts/` or `tools/` (only the parser, not the mounts)

**Interfaces:**
- Produces: `const char *root_device(const char *cmdline)`: `zyperos.root=`'s value, or the device found by the name `plastron-root` when there is none.

- [ ] **Step 1:** Failing test: `root_device("... zyperos.root=/dev/mmcblk0p4 ...")` is `/dev/mmcblk0p4`; a line without it falls back to the name lookup; a value longer than the buffer is refused (falls back).
- [ ] **Step 2:** Implement; the root update (`/games/.dev/root-update.squashfs`) now writes slot A's partition and is taken only when slot A boots.
- [ ] **Step 3:** Commit.

### Task 6a.5: System Update on the Brick

**Files:**
- Modify: `plastron/scripts/release.sh` (the Brick's system pack carries `Image.lz4`, `initramfs.cpio.gz`, `brick.dtb`, `Image`)
- Modify: `update/main.c` (before writing a slot's files: free space on the boot partition against their size, a clear stop when short)
- Test: `tools/update-check.sh`

- [ ] **Step 1:** Failing check: a pack whose files exceed the scratch boot folder's free space stops with "not enough room on the boot partition" and writes nothing; a Brick pack's four files land in `/b/`; a file in the scratch boot folder's `closed/` is the same after the apply.
- [ ] **Step 2:** Implement; run `make check-update check-sysupdate`.
- [ ] **Step 3:** Commit.

### Task 6a.6: The board's loose ends

**Files:**
- Modify: `plastron/buildroot/board/brick/third-party-packages.md` (the xz row)
- Modify: `plastron/buildroot/package/zyperos/zyperos.mk:141-150` (px2-shot, px2-input, px2-perf for the Pixel 2 only)
- Modify: `plastron/Makefile` (`release-brick` needs `brick-inputs` and `brick-kernel`, as `release` does)
- Modify: `plastron/buildroot/board/brick/rootfs-overlay/etc/inittab` (the shared `crashlog` step, so a panic or a reset before a start leaves `crash-*.txt` in the logs, which the log pack carries)
- Create: `plastron/buildroot/board/brick/u-boot/patches/0011-trimui-brick-fallback-slot-a.patch`; rebuild `board/brick/bootloader/u-boot-sunxi-with-spl.bin` with `scripts/build-brick-bootloader.sh`

- [ ] **Step 1:** `release-brick` builds to the end; `plastron/scripts/release.sh brick`'s package compare passes.
- [ ] **Step 2:** `strings` on the new boot loader shows the fallback's `/a/` paths.
- [ ] **Step 3:** The Pixel 2's plastron build: payload and image files the same as main's (the step 4 proof, 646 files).
- [ ] **Step 4:** Commit; open PR 6a.

---

## PR 6b: The closed files as the player's own copy

### Task 6b.1: The image without them

**Files:**
- Modify: `plastron/buildroot/package/pvr-ge8300/pvr-ge8300.mk` (headers and `.pc` to staging as now; the libraries to staging only; the target gets links into `/boot/closed/`)
- Modify: `plastron/buildroot/package/xr829-firmware/xr829-firmware.mk` (links in `/lib/firmware/` into `/boot/closed/`)
- Modify: `plastron/scripts/release.sh:108-122` (no PROPRIETARY.txt; a note that the image carries none)

- [ ] **Step 1:** Build; check that the root squashfs (`unsquashfs -l`) has the 13 names as symbolic links into `/boot/closed/` and none of the 13 files' bytes (no file in it has one of the 13 sha256 sums).
- [ ] **Step 2:** Commit.

### Task 6b.2: The missing-files screen

**Files:**
- Modify: `res/strings/boot.txt` (new `boot.closed_missing`: "The graphics and Wi-Fi files are missing. Run the ZyperOS Installer's Update on a computer.", 8 languages)
- Modify: `tools/genfirststart.py` (the Brick's frames for it, 1024x768, one per language)
- Modify: `plastron/buildroot/package/splash` (`splash say-missing`, the frame for `zylang`, English without it)

- [ ] **Step 1:** `make check-strings`; the generator's output frames checked by size and by a shot sheet for review.
- [ ] **Step 2:** Commit.

### Task 6b.3: The check at boot

**Files:**
- Modify: `plastron/buildroot/board/brick/rootfs-overlay/etc/init.d/brick` (after `/boot` is mounted, before the GPU driver loads: each of the 13 files present with its size from a list in the image; when one is not, the missing-files screen, the launcher not started, POWER powers off)
- Create: the list, `/usr/share/zyperos/closed.list` (name and size), generated from the two `files.sha256` and the files at build time

- [ ] **Step 1:** A host test of the check function on a fixture folder: all present passes; one missing, one short, an empty folder each fail and name the file.
- [ ] **Step 2:** Commit.

### Task 6b.4: The installer's step

**Files:**
- Create: `installer/src-tauri/src/closed.rs`
- Create: `installer/src-tauri/build.rs` additions (the list from `plastron/buildroot/package/pvr-ge8300/files.sha256` and `.../xr829-firmware/files.sha256`)
- Modify: `installer/src-tauri/src/install.rs`, `main.rs` (Brick install and Update: fetch first, then the image, then the files into p1's `closed/`)
- Modify: `installer/ui/` (one progress line, "Getting the Brick's graphics and Wi-Fi files", and its error words)
- Add dependency: `fatfs` (with long file names)

**Interfaces:**
- Produces: `fn fetch_all(cache: &Path, progress: impl Fn(usize, usize)) -> Result<Vec<PathBuf>, ClosedError>`; `fn write_into_boot(dev: &mut (impl Read + Write + Seek), p1_offset: u64, p1_len: u64, files: &[PathBuf]) -> Result<(), ClosedError>`; `ClosedError` names the file and says download, checksum, or room.

- [ ] **Step 1:** Failing tests: a sha256 mismatch fails and names the file; a cached file with the right sum is not fetched again; an offline fetch fails before any write; `write_into_boot` on a FAT image fixture adds `closed/` with the 13 names (long names intact) and leaves `a/` and `bootstate.txt` as they were; a fixture with a partial `closed/` is repaired.
- [ ] **Step 2:** Implement with a local HTTP server or `file://` base in the tests (the base URL a parameter, the real one a constant).
- [ ] **Step 3:** `cargo test`, `make check-installer`.
- [ ] **Step 4:** Commit.

### Task 6b.5: The installer takes an image from a file

**Files:**
- Modify: `installer/src-tauri/src/main.rs` (the path `ZYPEROS_BRICK_IMAGE` already takes, reached from the UI: a chosen `.img.xz`, checked against a `SHA256SUMS` beside it when there is one, its device read from its boot label with `cards::system_of`)
- Modify: `installer/ui/` ("Use an image file", under the device choice)

- [ ] **Step 1:** Failing tests: a file whose sum differs from its SHA256SUMS is refused and named; a Brick image is offered as the Brick; a file with no boot label is refused.
- [ ] **Step 2:** Implement; `cargo test`, `make check-installer`.
- [ ] **Step 3:** Commit; open PR 6b.

---

## PR 6c: The Brick's device code

### Task 6c.1: Charging, shared

**Files:**
- Create: `src/charging_rule.c`, `src/charging_rule.h`
- Modify: `src/device/pixel2.c:1940-1951`, `src/device/brick.c:1205-1244` (both call it; the Brick reads `axp2202-usb/online`)
- Test: `tools/battery-check.c` or the existing `check-battery`

**Interfaces:**
- Produces: `bool charging_from(int online, const char *status)`: online 0 is not charging whatever the status says; online 1 or -1 (no file) goes by "Charging" or "Full".

- [ ] **Step 1:** Failing cases: `(0, "Charging")` false, `(1, "Discharging")` false, `(1, "Full")` true, `(-1, "Charging")` true.
- [ ] **Step 2:** Implement; the Pixel 2's behavior the same (its cases pass unchanged).
- [ ] **Step 3:** Commit.

### Task 6c.2: Temperature, charge, Wi-Fi

**Files:**
- Modify: `src/device/profile.h` (`thermal_zones`: the zone types to read, NULL-ended; the Pixel 2's `soc-thermal`, `gpu-thermal`; the Brick's `cpu_thermal_zone`, `gpu_thermal_zone`)
- Create: `src/thermal.c` (`int thermal_max_c(const char *const *types, const char *sysroot)`) shared by both devices
- Modify: `src/device/brick.c` (temperature from it; charge reading from `axp2202-battery` as pixel2.c reads rk817's; Wi-Fi from `/sys/class/net/wlan0`; the ladders to `src/device/brick_levels.h`)
- Test: `tools/devices-check.c`, a thermal case with a fixture `sysroot`

- [ ] **Step 1:** Failing tests: `thermal_max_c` on a fixture with two zones returns the higher in °C; unknown types return -1.
- [ ] **Step 2:** Implement; `make check-devices check-heat check-battery`.
- [ ] **Step 3:** Commit; open PR 6c.

---

## PR 6d: The hardware report and the panel's line

### Task 6d.1: The report

**Files:**
- Create: `src/hwreport.c`, `src/hwreport.h`
- Modify: `src/xfer_screen.c:466-483` (about.txt's first lines: `Device: <profile name> (<id>)`, `Build: <version> <commit>`, then `Hardware:` and the report)
- Modify: `src/device/brick.c`, `src/device/pixel2.c` (`void plat_hw_facts(hw_facts *f)`)
- Test: `tools/logpack-check.c` (or the existing log pack check)

**Interfaces:**
- Produces: `typedef struct { char panel[64], gpu_kernel[64], gpu_user[96], wifi[64]; char inputs[8][64]; int n_inputs; int closed_present, closed_total; char boot_times[512]; } hw_facts;` and `size_t hwreport_build(const hw_facts *f, char *out, size_t n)`, which writes one `name: value` line each and `-` for what is unknown.

Where each fact comes from on the Brick: the panel from the kernel log's `brick panel:` line (`klogctl`, as `board/brick/probe/probe.c:54-75` reads it); the inputs from `/proc/bus/input/devices`' `N: Name=` lines; the GPU's kernel side from the kernel log's PVR line and its user side from the renderer strings the launcher logs at start; Wi-Fi from `/sys/class/net/wlan0/device/driver`'s name; the closed files' count from `closed.list`; boot times from `/tmp/boot-times` and the launcher's first frame.

- [ ] **Step 1:** Failing test: `hwreport_build` on filled facts gives the exact lines; on empty facts every value is `-`; a long input name is cut, never overflowing.
- [ ] **Step 2:** Implement; `make check-logpack` (or the check that covers `logpack.c`).
- [ ] **Step 3:** Commit.

### Task 6d.2: The panel at early boot

**Files:**
- Modify: `plastron/buildroot/board/brick/rootfs-overlay/etc/init.d/brick` (the `brick panel:` line from `dmesg` to `/tmp/panel`, then `splash say "<text>"`)
- Modify: `plastron/buildroot/package/splash` (`splash say TEXT`: one line at the panel's foot from a glyph atlas)
- Modify: `tools/genfirststart.py` (the atlas: printable ASCII in the launcher's own font, white on the frame's ground)
- Modify: `sd/brick/device.sh` (`device_log`: `/tmp/panel` and `/tmp/boot-times` into the logs folder)

- [ ] **Step 1:** Host test: the atlas has 95 glyphs of one height; `splash say` on a fixture frame buffer draws within the bottom 40 rows and nowhere else.
- [ ] **Step 2:** Commit; open PR 6d.

---

## PR 6e: Kit 2

### Task 6e.0: Kit builds of the installer

The release's installers carry only the updater's signature: macOS asks for *Open Anyway* and Windows for *Run anyway* the first time (installer/README.md). A kit's installers are built the same way, so testers meet the same first open, and nothing new is needed from Tarik but his OK to publish the kit.

**Files:**
- Modify: `.github/workflows/release.yml` (also on tags `kit-*`: the same three builds, signed with the run's throwaway key, no `latest.json`; the draft published with `--prerelease --latest=false`)
- Modify: `tools/release.sh` (`--kit NAME`: the draft `kit-<name>` with the kit's images, SHA256SUMS and its test list as notes; the installer's version `<next>-kit.<n>`, below the next release, so a kit installer updates itself to that release when it is out)
- Modify: `installer/src-tauri/src/release.rs` (a kit build reads its own kit's release, the tag set at build time by `ZYPEROS_KIT_TAG`, where a release build reads the latest: a tester downloads only the installer, and the image comes as a release's does)

- [ ] **Step 1:** Failing test in `release.rs`: with a kit tag set, the release asked for is that tag's, prereleases included; without one, the latest as now.
- [ ] **Step 2:** A pull request that changes `release.yml` builds the apps (as today) and a dry run of the kit path stops before any upload.
- [ ] **Step 3:** Commit; the kit tag is pushed only in Task 6e.1, on Tarik's OK.

### Task 6e.1: The image and its page

- [ ] **Step 1:** Build `release-brick` from main once 6a to 6d are merged; record the image's sha256.
- [ ] **Step 2:** Update `brick-kit-2-test-list.md` (step 1: the kit's installer, its first open as installer/README.md says, and "Use an image file").
- [ ] **Step 3:** Ask the coordinator: the kit's page (a draft or pre-release named `brick-kit-2`, the image, SHA256SUMS, what changed, the list) is published only on Tarik's OK.

---

## After kit 2

The Smart Pro's kit 2b (spec 6c) starts here. Kit 3 tests System Update between slots with a second pack.
