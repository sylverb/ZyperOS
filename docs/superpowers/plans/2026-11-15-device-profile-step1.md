# Device profile (step 1) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** One constant `device_profile` per handheld (Pixel 2, Brick, and the Mac host's `desktop`), which the launcher reads instead of guessing the device, with the Pixel 2's every pixel unchanged.

**Architecture:** Profiles are plain data in `src/device/<id>_profile.c` (no SDL, no hardware), reached through `plat_profile()` in a new portable `src/profile.c`. The device build links its one profile; the host build links all three and picks one with `--device`. Capability questions that were constants in the device files are answered from the profile; live questions (Wi-Fi present, charge limit writable, MENU held) stay in the device files.

**Tech Stack:** C (gnu11), SDL2 only where it already is, the host checks in `tools/*-check.c` run by `make check`, Python 3 for the lint, GitHub Actions.

**Spec:** `docs/superpowers/specs/2026-11-14-device-modular-design.md` (sections 1, 2, 9 step 1; Constraints "The design system's round 3"; Calls made).

## Global Constraints

- Code starts only after the main.c split round 2 (PRs D and E) and the design system round 2 PR have merged. Rebase branch `device-modular` onto the new `origin/main` first; the spec and this plan go to `main` in the same PR.
- The Pixel 2 does not change: `make pixel-gate BASE=origin/main GATE='--lang en'` and `GATE='--lang en --theme dark'` report 0 frames changed (unsteady frames are fine). No frame rate loss: profiles are read, never computed, per frame.
- New C files start `/* SPDX-License-Identifier: GPL-3.0-only */` and follow the repository's comment style (prose saying why).
- `src/device/profile.h` and every `*_profile.c` include no SDL header, so SDL-free checks link them.
- The profile states physical facts only for the screen: no canvas size and no scale (agreed with the design system, 2026-11-15).
- On-screen glyphs never use `cap_rgb`; it is for the 3D handheld only. Icons come from the closed set NONE, SQUARE, PLAY, DOTS, STICK; a vendor mark is never an icon (the Brick's MENU is the label "MENU", icon NONE).
- Never run `zyperos.elf` over SSH; this step needs no device at all.
- Commits and the PR in Tarik's first person, no AI mention, no Co-Authored-By.

## Calls this plan makes (recorded in the spec in Task 7)

- **The desktop driver split moves to step 6.** `src/device/brick.c` keeps its host half for now (the Mac build still compiles it); the desktop *profile* is new here. Step 6 rewrites `brick.c` for the image anyway.
- **The generated `<id>.mk` / `<id>.h` move to step 4**, with their first consumer (plastron's device symbol).
- **`--fn-volume` stays a narrow shot override** (the four chords and the tutorial handheld on top of the active profile) until step 5 moves the shots to `--device pixel2` with a reviewed sheet. `--device pixel2` is new and has no gate baseline to match.
- **The tutorial shows the handheld when the profile has one and brightness is MENU+VOL.** The second clause only exists for the desktop profile, which borrows the Pixel 2's art for the Controls tab as today's host does; step 5 removes it when the Brick gets its own model.
- **`plat_*` capability functions stay as the read path**, defined once in `src/profile.c` from the profile, so the 40-odd call sites do not churn.
- **Button places on the handheld come with step 2's `td_model`**; this step's table carries the label, cap, icon and cap color the design system's glyph needs.
- **The desktop profile answers GPU shaders "yes"** (today: only when headless). Shots are headless, so nothing in the gate moves; the windowed Mac run now offers those shaders too.
- **Execution: native, then one fresh review of the whole branch** before the PR. The tasks share `src/main.c` and the profile's interfaces, so one implementer with the whole context is cheaper and safer than seven hand-offs; a pixel-gate failure, not a review, is the costly mistake here.
- **The profile's screen facts are not read by `pixel2.c` yet**: QEMU's landscape display runs the Pixel 2 build, and `pixel2.c` keeps taking the panel from SDL at run time.

## Review Focus

1. A Pixel 2 on the shipped image: the tutorial's MENU lesson still waits for MENU's release, brightness is still MENU+VOL, the Controls page still lists L2+R2 and MENU+START. (Task 3 pins these per profile in `tour-check` and `controls-check`.)
2. `--device` given an unknown id, or a device build asked for another device's profile: a one-line error naming the known ids and exit status 2, never a launcher drawn with the wrong buttons. (Task 5.)
3. The Brick build still compiles and links with its own profile: plastron's `zyperos.mk` passes `DEVICE=brick` to `mk/cross.mk`. (Task 6's CI job compiles `src/profile.c` with `src/device/brick_profile.c` and `-DZYPEROS_PROFILE=device_profile_brick`.)
4. Developer Mode's unlock: on the Pixel 2 it still needs L2 and R2 held as taps for 5 s; on a device whose L2 and R2 are plain buttons it reads them held. Neither unlocks from one shoulder. (Task 4.)
5. A card's existing `.userdata/brick` or `.userdata/pixel2` folder and System Update's device name stay what they are today on each device and on the host. (Task 2 pins `userdata` per profile.)

---

### Task 1: The profile and the three profiles

**Files:**
- Create: `src/device/profile.h`, `src/device/pixel2_profile.c`, `src/device/brick_profile.c`, `src/device/desktop_profile.c`, `src/profile.c`, `tools/devices-check.c`
- Modify: `mk/cross.mk` (SRC and CFLAGS), `mk/native.mk` (SRC and CFLAGS), `Makefile` (`check-devices` target, add to `CHECKS`)

**Interfaces:**
- Produces, in `src/device/profile.h`:
  - `typedef enum { DEV_BRIGHT_KEYS, DEV_BRIGHT_MENU_VOL } dev_bright;`
  - `typedef struct { dev_bright bright; bool menu_on_release, lr2_taps, home_chord, battery_chord; } dev_chords;`
  - `typedef enum { DEV_CAP_ROUND, DEV_CAP_PILL, DEV_CAP_ROCKER, DEV_CAP_DPAD, DEV_CAP_SHOULDER, DEV_CAP_STICK } dev_cap;`
  - `typedef enum { DEV_ICON_NONE, DEV_ICON_SQUARE, DEV_ICON_PLAY, DEV_ICON_DOTS, DEV_ICON_STICK } dev_icon;`
  - `typedef struct { in_button id; const char *label; dev_cap cap; dev_icon icon; uint32_t cap_rgb; } dev_button;` (`cap_rgb` 0xRRGGBB, 0 for "the body's dark")
  - `typedef enum { DEV_WIFI_NONE, DEV_WIFI_ADAPTER, DEV_WIFI_BUILTIN } dev_wifi;`
  - `typedef struct device_profile { const char *id, *name, *userdata, *boot_label; struct { int panel_w, panel_h, turn; float active_w_mm, active_h_mm; int inset_t, inset_r, inset_b, inset_l; int aspect_w, aspect_h; bool still; } screen; dev_chords chords; const dev_button *buttons; int n_buttons; bool handheld, bluetooth, gpu_shaders, speaker_mono, player2, usb_shared_port; dev_wifi wifi; } device_profile;`
  - `extern const device_profile device_profile_pixel2, device_profile_brick, device_profile_desktop;`
- Produces, in `src/profile.c` (declared in `src/device/profile.h`):
  - `const device_profile *plat_profile(void);` the active profile, never NULL
  - `bool plat_profile_select(const char *id);` false when no linked profile has that id
  - `const dev_button *dev_button_of(const device_profile *p, in_button b);` NULL when the device has no such control

- [ ] **Step 1: Write the failing check** `tools/devices-check.c` (plain C, `assert`-style like `tools/tour-check.c`, prints `devices: N checks passed`). It pins:

```c
/* every profile */
assert(p->id && p->name && p->userdata && p->boot_label && p->n_buttons > 0);
assert(p->screen.aspect_w * 3 == p->screen.aspect_h * 4);          /* 4:3 only */
for each button: assert(b->label && *b->label);                     /* every control printed */
/* no in_button twice in one table */
/* today's answers, which the pixel gate depends on */
P2: id "pixel2", userdata "pixel2", boot_label "PX2BOOT", panel 480x640, turn 270 (the -90 of pixel2.c),
    active 48.8 x 36.6 mm, bright MENU_VOL, menu_on_release, lr2_taps, home_chord, battery_chord,
    handheld, !bluetooth, gpu_shaders, speaker_mono, player2, usb_shared_port, wifi ADAPTER,
    dev_button_of(P2, IN_MENU): label "MENU", cap ROUND, icon DOTS; dev_button_of(P2, IN_BRIGHTUP) == NULL
BR: id "brick", userdata "brick", boot_label "BRICKBOOT", panel 1024x768, turn 0, active 65.0 x 48.8 mm,
    bright KEYS, !menu_on_release, !lr2_taps, !home_chord, !battery_chord, !handheld, bluetooth,
    !gpu_shaders, !speaker_mono, !player2, !usb_shared_port, wifi BUILTIN,
    dev_button_of(BR, IN_MENU): label "MENU", icon NONE; IN_BRIGHTDN label "F1"; IN_BRIGHTUP label "F2"
DT: id "desktop", userdata "brick" (today's host path), chords == BR's, handheld (the Controls tab's
    art, as today), bluetooth, gpu_shaders (today's headless answer), !speaker_mono, !player2,
    !usb_shared_port, wifi BUILTIN
/* selection (built with all three) */
assert(plat_profile() == &device_profile_desktop);
assert(plat_profile_select("pixel2") && plat_profile() == &device_profile_pixel2);
assert(!plat_profile_select("gameboy") && plat_profile() == &device_profile_pixel2);
```

- [ ] **Step 2: Add `check-devices` to the Makefile and run it:** `make check-devices`. Expected: compile error, `profile.h` not found.

- [ ] **Step 3: Write `src/device/profile.h` and the three profiles** with the values above. Button tables: Pixel 2 — d-pad (DPAD), A B X Y (ROUND), L1 R1 L2 R2 (SHOULDER), SELECT and START (PILL), MENU (ROUND, DOTS), VOL+ and VOL- (ROCKER), POWER (PILL). Brick — the same face and shoulders, SELECT (ROUND, SQUARE), START (ROUND, PLAY), MENU (ROUND, NONE), F1 on IN_BRIGHTDN and F2 on IN_BRIGHTUP (PILL), VOL+/VOL- (ROCKER), POWER (ROUND). Desktop — the Brick's table. Inset fields 0, `still` true on the Pixel 2 only.

- [ ] **Step 4: Write `src/profile.c`.** The default profile is the symbol the build names: `-DZYPEROS_PROFILE=device_profile_$(DEVICE)` in `mk/cross.mk`, `-DZYPEROS_PROFILE=device_profile_desktop -DZYPEROS_PROFILES_ALL` in `mk/native.mk`. With `ZYPEROS_PROFILES_ALL` the selection table holds all three; without it, only the default. Add `src/device/$(DEVICE)_profile.c` to `mk/cross.mk`'s SRC and all three profile files to `mk/native.mk`'s SRC.

- [ ] **Step 5: Run** `make check-devices`. Expected: `devices: ... checks passed`.

- [ ] **Step 6: Run** `make native && make check`. Expected: every check passes (nothing reads the profile yet).

- [ ] **Step 7: Commit:** `git commit -m "Devices: a profile for each handheld, and the host's"`

### Task 2: Capability answers from the profile

**Files:**
- Modify: `src/profile.c` (the wrappers), `src/device/pixel2.c` and `src/device/brick.c` (delete their definitions of the six functions below), `src/main.c` `up_device()` (or wherever PR E leaves it), `tools/devices-check.c`

**Interfaces:**
- Consumes: Task 1's `plat_profile()`.
- Produces: `plat_has_bluetooth`, `plat_gpu_shaders`, `plat_speaker_mono`, `plat_brightness_on_volume`, `plat_battery_lights`, `plat_player2`, unchanged prototypes in `src/platform.h`, now defined only in `src/profile.c` as `p->bluetooth`, `p->gpu_shaders`, `p->speaker_mono`, `p->chords.bright == DEV_BRIGHT_MENU_VOL`, `p->chords.battery_chord`, `p->player2`. `plat_has_wifi`, `plat_charge_limit_supported`, `plat_menu_held`, `plat_tap_held` stay in the device files (live).

- [ ] **Step 1: Extend the check:** for each of the three profiles, select it and assert each wrapper equals its field (link `src/profile.c` with SDL's cflags only; it calls no SDL).
- [ ] **Step 2: Run** `make check-devices`. Expected: link error (duplicate or missing symbols) until Step 3.
- [ ] **Step 3: Move the six definitions** from `pixel2.c` and `brick.c` into `src/profile.c`. `up_device()` returns `plat_profile()->userdata` instead of the basename of `P_USERDATA` (same string on every device and on the host).
- [ ] **Step 4: Run** `make check-devices && make native && make check`. Expected: all pass.
- [ ] **Step 5: Run the pixel gate** (Global Constraints). Expected: 0 changed.
- [ ] **Step 6: Commit:** `git commit -m "Devices: the constant answers come from the profile"`

### Task 3: Four chords in place of `fn_volume`

**Files:**
- Modify: `src/tour.h`, `src/tour.c`, `src/tourdraw.c`, `src/tourdevice.c`, `src/tourdevice.h`, `src/controls.h`, `src/controls.c`, every caller of `tour_start`, `tour_pose`, `ctl_set_fn_volume`, `ctl_set_battery_lights` (today `src/main.c`, `src/ingame_screen.c`, and the tour screen file PR D/E creates), `src/profile.c`, `tools/tour-check.c`, `tools/controls-check.c`, the Makefile rules of those two checks (add the profile sources)

**Interfaces:**
- Consumes: Task 1's `dev_chords`, `device_profile`.
- Produces:
  - `void tour_start(tour *t, uint32_t now, const dev_chords *c, bool handheld);`
  - `void tour_pose(tour *t, tour_step step, uint32_t now, int ms, int done_ms, const dev_chords *c, bool handheld);`
  - `tour` loses `fn_volume` and gains `dev_bright bright; bool menu_release; bool handheld;`
  - `bool tourdev_shown(const tour *t)` reads `t->handheld` and the lesson step
  - `void ctl_set_chords(const dev_chords *c);` replaces `ctl_set_fn_volume` and `ctl_set_battery_lights`
  - in `src/profile.c`: `dev_chords dev_chords_fn_volume(dev_chords base);` (bright MENU_VOL, menu_on_release, home_chord set; the rest kept) and `bool dev_tutorial_handheld(const device_profile *p, const dev_chords *c);` (`p->handheld && c->bright == DEV_BRIGHT_MENU_VOL`, with the step 5 comment)

Today's `fn_volume` uses map as: MENU acts on release (`tour.c` lesson press time) → `menu_release`; the demo press, `tour_hands`, the wrong-press hints, the chord glyph, `line_fn` → `bright`; `tourdev_shown` and the centred, unshifted block → `handheld`; the Controls pages' "MENU+VOL" / "F1/F2" and Muse's lock wording → `bright`; the Home row → `home_chord`; the L2+R2 row → `battery_chord`.

- [ ] **Step 1: Change the checks first.** `tools/tour-check.c` runs its lesson and wrong-press cases once with `device_profile_pixel2.chords` and handheld true, and once with `device_profile_brick.chords` and handheld false, expecting what its `fn_volume` true and false cases expect today. `tools/controls-check.c` loops over the two profiles' chords where it looped over its `pixel` bool, with the same expectations.
- [ ] **Step 2: Run** `make check-tour check-controls`. Expected: compile errors on the new signatures.
- [ ] **Step 3: Change `tour`, `tourdraw`, `tourdevice` and `controls`** to the interfaces above, by the mapping above.
- [ ] **Step 4: Change the callers.** Live: `tour_start(&t, now, &plat_profile()->chords, dev_tutorial_handheld(plat_profile(), &plat_profile()->chords))` and `ctl_set_chords(&plat_profile()->chords)`. Shots with `--fn-volume`: `dev_chords c = dev_chords_fn_volume(plat_profile()->chords)` passed with handheld true; without it, the profile's.
- [ ] **Step 5: Run** `make check-tour check-controls check-tour-shots check-controls-shots`. Expected: pass, with today's shot expectations untouched.
- [ ] **Step 6: Run** `make check` and the pixel gate. Expected: all pass, 0 changed.
- [ ] **Step 7: Commit:** `git commit -m "Tutorial and Controls: the device's chords, not one Pixel 2 flag"`

### Task 4: Two fixes on the way

**Files:**
- Modify: `src/profile.c`, the About page's unlock in `src/main.c` (or its new file), the in-game menu's `g_menu_side` assignment (`ctlscr_side`, in `src/main.c` or `src/ingame_screen.c` after PR D/E), `tools/devices-check.c`, `tools/controls-shots-check.sh`

**Interfaces:**
- Consumes: Task 1's profile, Task 3's chords.
- Produces: `bool dev_l2r2_held(const dev_chords *c, bool tap_l2, bool tap_r2, bool down_l2, bool down_r2);` — taps when `c->lr2_taps`, the buttons' own state otherwise; both needed either way.

- [ ] **Step 1: Write the failing cases** in `tools/devices-check.c`: Pixel 2 chords: `dev_l2r2_held(c, 1, 1, 0, 0)` true, `(c, 1, 0, 0, 0)` false, `(c, 0, 0, 1, 1)` false; Brick chords: `(c, 0, 0, 1, 1)` true, `(c, 0, 0, 1, 0)` false, `(c, 1, 1, 0, 0)` false.
- [ ] **Step 2: Run** `make check-devices`. Expected: fails (undefined `dev_l2r2_held`).
- [ ] **Step 3: Implement it**, and use it in the About page's `devmode_hold_step` call with `plat_tap_held(IN_L2/R2)` and that loop's `in_state` `down[IN_L2/R2]`.
- [ ] **Step 4: The Controls tab draws the handheld only when `plat_profile()->handheld`;** without it the panel keeps its size and the column is empty (no Pixel 2 on a Brick).
- [ ] **Step 5: Run** `make check-devices check-controls-shots && make check` and the pixel gate. Expected: pass, 0 changed (the desktop profile keeps the handheld). Task 5 adds the Brick's shot case.
- [ ] **Step 6: Commit:** `git commit -m "Developer Mode unlocks where L2 and R2 are plain buttons; the Controls tab draws no other device"`

### Task 5: `--device` on the host

**Files:**
- Modify: `main()`'s option loop (in `src/main.c`, or the shot harness file PR E creates), `tools/tour-shot-check.py`, `tools/controls-shots-check.sh`, `docs/building.md` (a short "Devices" section: what a profile is, `--device`, `make check-devices`)

**Interfaces:**
- Consumes: Task 1's `plat_profile_select`.
- Produces: the flag `--device ID`, read before anything else draws or reads a capability.

- [ ] **Step 1: Add the shot check cases.** In `tools/tour-shot-check.py`: the first three lesson frames shot with `--device pixel2` equal, pixel for pixel, the same frames shot with `--fn-volume` (the tutorial reads only chords and the handheld); with `--device brick` they equal the frames shot with neither flag. In `tools/controls-shots-check.sh`: the in-game Controls tab shot with `--device brick` has none of the handheld's lit lime in the column box it already measures (Task 4's fix).
- [ ] **Step 2: Run** `make check-tour-shots`. Expected: fails, unknown flag.
- [ ] **Step 3: Implement `--device`:** `if (!plat_profile_select(id)) { fprintf(stderr, "--device: no profile \"%s\" (pixel2, brick, desktop)\n", id); return 2; }`, the list built from the linked table.
- [ ] **Step 4: Run** `make check-tour-shots check-controls-shots && make check`. Expected: pass.
- [ ] **Step 5: Commit:** `git commit -m "Host: --device takes any handheld's profile"`

### Task 6: What the system has, and the device files compiled

**Files:**
- Modify: `src/profile.c` (`plat_system_has`), `src/platform.h` (its prototype), the Flash Drive offer `usb_drive_offered()` in `src/main.c`, `hotspot_available()` in `src/hotspot.c`, the empty home's shared-port guess in `src/shelf_draw.c`, `Makefile` (`check-device-compile`, the lint in `check-devices`), `.github/workflows/check.yml` (a job `devices`)
- Create: `tools/devices-lint.py`

**Interfaces:**
- Produces: `bool plat_system_has(const char *part);` for `"usbdrive"` (`/usr/sbin/usbdrive`, X_OK) and `"hostapd"` (`/usr/sbin/hostapd`, X_OK); any other part false. Step 4 adds the image's `has` line in front of these checks.

- [ ] **Step 1: Write the lint** `tools/devices-lint.py`, run by `check-devices`: in `src/*.c` and `src/*.h` outside `src/device/` and `src/profile.c`, fail on `access("/usr/sbin/`, on `plat_usb_peer() != USB_PEER_UNKNOWN` used as a capability, and on the string literals `"pixel2"` and `"brick"`, except an allow list in the script naming each remaining use with its reason (the shot harness's `--ways ...,brick` until step 5, i18n tables).
- [ ] **Step 2: Run** `make check-devices`. Expected: fails, naming `src/main.c` (usbdrive), `src/hotspot.c` (hostapd) and `src/shelf_draw.c` (the peer guess).
- [ ] **Step 3: Fix them:** `usb_drive_offered()` uses `plat_system_has("usbdrive")`; `hotspot_available()` uses `plat_system_has("hostapd")` beside its live wlan0 probe; the empty home reads `plat_profile()->usb_shared_port` and `plat_profile()->wifi == DEV_WIFI_BUILTIN` where it guessed (the desktop profile answers as the guess did on the host).
- [ ] **Step 4: Run** `make check-devices && make check` and the pixel gate. Expected: pass, 0 changed.
- [ ] **Step 5: Add `check-device-compile`:** on Linux, `cc -std=gnu11 -D_GNU_SOURCE -fsyntax-only -Isrc $(pkg-config --cflags sdl2)` over `src/device/pixel2.c` and `src/device/brick.c`, `src/profile.c` with each `-DZYPEROS_PROFILE=device_profile_<id>` and its profile file, and Diatom's `port/pixel2.c` and `port/brick.c` with their Makefile's include flags; elsewhere it prints `skipped: device files compile on Linux (CI job devices)`. Not in `CHECKS` (the macOS job cannot run it).
- [ ] **Step 6: Add the CI job** `devices` to `.github/workflows/check.yml`: `ubuntu-24.04`, `actions/checkout@v4`, `sudo apt-get install -y libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libegl-dev libgles-dev libgbm-dev libdrm-dev libasound2-dev libsqlite3-dev`, then `make check-device-compile`.
- [ ] **Step 7: Run it locally in Docker** (`docker run --rm -v "$PWD":/z -w /z ubuntu:24.04 sh -c 'apt-get update -qq && apt-get install -y -qq build-essential pkg-config <the packages> && make check-device-compile'`). Expected: no errors (warnings allowed, as `brick.c`'s unused `jack_fd`).
- [ ] **Step 8: Commit:** `git commit -m "Devices: what the system has, a lint, and every device file compiled in CI"`

### Task 7: The record, and the PR

**Files:**
- Modify: `docs/superpowers/specs/2026-11-14-device-modular-design.md` ("Calls made in this design": the plan's six calls; step 1 marked done), `DESIGN.md` only where a section names `fn_volume` or "the Pixel 2 only" for the tutorial handheld (now "a device with a handheld")

- [ ] **Step 1: Update the spec and DESIGN.md** as listed.
- [ ] **Step 2: Run** `make check` and both pixel gate runs once more. Expected: all pass, 0 changed.
- [ ] **Step 3: Commit, push the branch, open the PR** to `main` with the pixel gate results in the body; tell the coordinator when CI is green (the coordinator merges).
