# Systems, cores and power by capability (step 3) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** What a system needs, what a device's cores are tuned to, and a device's power steps become stated facts: a `needs|TAG|gpu-frames` line hides a shelf on a device without GPU frames and refuses its games with a reason, the core option defaults tuned on the Pixel 2 become its profile's (read at run time, no longer seeded), and Diatom's power steps become each port's data. The Pixel 2 does not change.

**Architecture:** The device profile (`src/device/profile.h`) gains `gpu_frames` and a table of core option defaults; `config/systems.cfg` gains `needs` lines, which `src/config.c` reads into `system_cfg`. A small pure module, `src/coreopts.c`, holds every device's core option defaults, merges them under the player's own rows at run time (sorted by key, as the database's scan was), and deletes once the rows 1.0.5 seeded. Diatom's ports state their clocks as a `diatom_power_profile` (`diatom/port/port_power.h`), which one inline function writes.

**Tech Stack:** C (gnu11), SQLite (the library database), the host checks, the pixel gate, Diatom's own suite.

**Spec:** `docs/superpowers/specs/2026-11-14-device-modular-design.md` (section 4, "Systems, cores and power by capability"; section 9 step 3). Builds on steps 1 (PR #172) and 2 (PR #175).

## Global Constraints

- Branch `device-caps` from the `main` that has #175 (`f802dbb`).
- The Pixel 2 does not change: `make pixel-gate BASE=origin/main GATE='--lang en'` reports 0 frames changed; `--coreopts TAG` prints the same lines in the same order as `main` for every tag in `config/systems.cfg`, on a fresh card and on a card 1.0.5 seeded; Diatom's Pixel 2 port writes the same clock files with the same values in the same order.
- The host's `desktop` profile copies today's answers (GPU frames on, the Pixel 2's core option defaults), so host shots and checks keep their pixels; `--device brick` shows the Brick's.
- No player setting is lost: a row the player changed survives the migration on every device.
- Player-visible text in all eight languages (`res/strings/*.txt`: en, zh-Hans, es, pt-BR, fr, de, ja, it), in the words the other launch messages use; a sheet of every player-visible change goes to the coordinator.
- Never run `mk/fetch-vendor.sh`; never take `/tmp/plastron-build-lock` by hand.
- New C files start with the GPL-3.0-only SPDX line; commits and the PR in Tarik's first person, no AI mention.

## Calls this plan makes

- **The cores' architecture (`vendor/<arch>/cores`), `-mcpu` from the profile and the payload's cores per device move to step 4.** All three are build and payload work whose only reader of the device's architecture is the generated `build/device/<id>.mk` that step 1 already moved to step 4; done here, the architecture would be written by hand in a second place, and a renamed cores release is a release action of its own. Step 3 keeps the launcher, Diatom and the library database.
- **Every core option default moves out of the seeded library**, not only the Panfrost ones: the device-neutral ones (Game Boy model, frameskips, ScummVM's buttons, MAME's screens, the Neo Geo CD's BIOS, PicoDrive's aspect, the Dreamcast's real BIOS) into `src/coreopts.c`'s table for every device, and the ones measured on the Pixel 2 or its motor into its profile. So a row in `library.db` under `coreopt.` is only ever the player's.
- **The Pixel 2's table is linked on every device**, because the one-time migration on any device deletes what 1.0.5 seeded, which was the Pixel 2's (a card moves). The profiles that use it are the Pixel 2's and the host's.
- **The Brick declares no GPU frames and no core option defaults of its own yet** (spec section 6: GPU frames off until a GPU core runs there), and no power steps: its port keeps `diatom_port_power` a no-op until its clocks are measured.

## Review Focus

1. A card 1.0.5 seeded, moved to a Brick: no Pixel 2 tuning left in its rows, the player's own changes kept. (Task 4's migration test.)
2. A player who chose a value equal to the old default: the migration deletes the row, and the device's default (the same value on the Pixel 2) comes back. (Task 4.)
3. A Dreamcast game in Favorites, Recent or resumed at boot on a device without GPU frames: refused under its name with the reason, never sent to Diatom. (Task 3.)
4. More default and player rows than the merged set holds: the player's rows are kept first and a default that does not fit is dropped with a log line, never half a pair. (Task 4.)
5. A `needs` line for a tag no `sys` line has, or a need the launcher does not know: ignored, the shelf shown. (Task 2.)

---

### Task 1: The profile's GPU frames

**Files:**
- Modify: `src/device/profile.h` (`bool gpu_frames;` beside `gpu_shaders`, with its comment: "Diatom gives cores that ask for them a GPU to draw on (SET_HW_RENDER)"), `src/device/pixel2_profile.c` (`true`), `src/device/desktop_profile.c` (`true`, today's host), `src/device/brick_profile.c` (`false`), `src/profile.c` (`bool plat_gpu_frames(void)`), `tools/devices-check.c`

**Interfaces:**
- Produces: `bool plat_gpu_frames(void);` in `src/device/profile.h`, answered from `plat_profile()->gpu_frames`.

- [ ] **Step 1: Write the cases** in `tools/devices-check.c`: pixel2 `gpu_frames` true, brick false, desktop true; after `plat_profile_select("brick")`, `!plat_gpu_frames()`; back to desktop, true.
- [ ] **Step 2: Run** `make check-devices`. Expected: FAIL (no field).
- [ ] **Step 3: Implement** the field, the three values and `plat_gpu_frames`.
- [ ] **Step 4: Run** `make check-devices`. Expected: pass.
- [ ] **Step 5: Commit** "Profiles: GPU frames".

### Task 2: `needs` lines in systems.cfg

**Files:**
- Modify: `config/systems.cfg` (a commented block after the `power` lines: `needs|DC|gpu-frames`, `needs|NAOMI|gpu-frames`, `needs|AW|gpu-frames`, `needs|N64|gpu-frames`, `needs|PSP|gpu-frames`, `needs|SATURN|gpu-frames`, saying their cores ask for GPU frames at SET_HW_RENDER and a device without them hides the shelf), `src/config.h` (`bool needs_gpu_frames;` in `system_cfg`), `src/config.c` (`needs` joins the three-field `extra` kinds; the value `gpu-frames` sets the flag, any other value is ignored)
- Test: `tools/config-check.c` (`make check-config`)

**Interfaces:**
- Produces: `system_cfg.needs_gpu_frames`.

- [ ] **Step 1: Write the cases:** loading `config/systems.cfg`, exactly DC, NAOMI, AW, N64, PSP and SATURN have `needs_gpu_frames`; a scratch file with `needs|NES|teleporter` and `needs|NOPE|gpu-frames` loads with no flag set and no other field changed.
- [ ] **Step 2: Run** `make check-config`. Expected: FAIL.
- [ ] **Step 3: Implement** the lines and the parser.
- [ ] **Step 4: Run** `make check-config check-home check-controls check-remap check-sysmark check-tour` (every check that loads systems.cfg). Expected: pass.
- [ ] **Step 5: Commit** "Systems: which need GPU frames".

### Task 3: No GPU-frame shelves where there are none

(Filled in from the map of every place a system is listed: see below.)

### Task 4: Core option defaults per device, at run time

**Files:**
- Create: `src/coreopts.h`, `src/coreopts.c` (the merge, the every-device table, the migration), `src/device/pixel2_coreopts.c` (the Pixel 2's table), `tools/coreopts-check.c`
- Modify: `src/device/profile.h` (`typedef struct { const char *key, *value; } dev_default;` and `const dev_default *coreopts; int n_coreopts;`, with `extern const dev_default dev_coreopts_pixel2[]; extern const int dev_n_coreopts_pixel2;`), the three profiles (Pixel 2 and desktop point at it; the Brick `NULL, 0`), `src/db.c` (the `coreopt.` rows leave `library_defaults`; the comments that explain each value move with it), `src/platform.c` (`coreopts_load` and `coreopt_nth` over the merged set; `COREOPT_MAX` 96), `src/main.c` (the migration once at start, before the first game, not in a shot, logged as autozone's is), `tools/db-check.c` (its frameskip and PSP cases move to `coreopts-check`), `Makefile` (`check-coreopts` in `CHECKS`, `src/coreopts.c` and `src/device/pixel2_coreopts.c` in `DEVICE_PROFILES`), `mk/native.mk` and `mk/cross.mk` (both new files in every build), `docs/building.md` (its row)

Every-device table (`coreopts.c`), as `src/db.c` has them today: `coreopt..mgba_sgb_borders`, `coreopt.GB.mgba_gb_model`, `coreopt.GB.mgba_gb_colors`, `coreopt..pcsx_rearmed_frameskip_type`, `coreopt..picodrive_frameskip`, `coreopt..mgba_frameskip`, `coreopt..snes9x_2010_frameskip`, `coreopt..genesis_plus_gx_frameskip`, `coreopt..picodrive_aspect`, `coreopt..reicast_hle_bios`, the five `coreopt.SCUMMVM.scummvm_mapper_*`, the two `coreopt..mame2003-plus_skip_*`, `coreopt..neocd_bios`. The Pixel 2's (`pixel2_coreopts.c`): `coreopt..reicast_internal_resolution`, `coreopt..mupen64plus-43screensize`, `coreopt.N64.mupen64plus-EnableFBEmulation`, `coreopt.N64.mupen64plus-pak1`, the seven `coreopt.N64.parallel-n64-*`, `coreopt.PS1.pcsx_rearmed_analog_combo`, `coreopt..opera_dsp_threaded`, the fourteen `coreopt.PSP.ppsspp_*`, `coreopt.DOOM.prboom-rumble`.

**Interfaces:**
- Produces (in `src/coreopts.h`):
  - `typedef struct { char tag[8]; char kv[192]; } coreopt;` (today's `platform.c` entry)
  - `bool coreopt_add(coreopt *set, int *n, int max, const char *key, const char *value);` — adds "coreopt.<tag>.<opt>"=value unless `set` has that tag's option already; false, nothing added, for a key with no tag dot, a tag of 8 or more characters, a pair that does not fit, or a full set
  - `void coreopt_defaults(coreopt *set, int *n, int max, const dev_default *dev, int n_dev);` — the device's, then every device's, each only where `set` has none; then `set` sorted by its full key
  - `int coreopt_nth(const coreopt *set, int n, const char *tag, int want, const char **out);` — today's precedence (globals, then `tag`'s), moved from `platform.c`
  - `bool coreopts_migrate(db *lib);` — once (`coreopts.runtime_106` set to "1"; outside the `coreopt.` prefix): deletes each `coreopt.` row whose value equals the every-device table's or `dev_coreopts_pixel2`'s for that key; true on the run that did it
  - `const dev_default *coreopt_shared(int *n);` — the every-device table, for the checks

- [ ] **Step 1: Record today's output** with `main`'s build: for every tag in `config/systems.cfg`, `--coreopts TAG` on a fresh scratch card, into `build-native/coreopts-main/<tag>.txt` (the scratch card as `tools/dslang-check.sh` makes one).
- [ ] **Step 2: Write `tools/coreopts-check.c`:** on the Pixel 2's defaults, a player's `coreopt..mgba_frameskip=disabled` wins over the default; `coreopt.GB.mgba_gb_model` is sent for GB and not for GBC; a device default overrides an every-device one of the same key; the merged set is sorted by key; an over-long key and a tag of 8 characters are refused whole; a full set keeps the player's rows and drops a default; the frameskip and PSP cases `db-check` had (keys and values written out); with a scratch library seeded as 1.0.5 seeded (every key of both tables at its value, plus `coreopt..mgba_frameskip=disabled` and `coreopt.N64.parallel-n64-pak1=memory`), `coreopts_migrate` deletes every row equal to a default, keeps the two changed ones, sets `coreopts.runtime_106`, and a second call changes nothing and returns false; the Brick's profile has no defaults of its own and still gets the every-device ones.
- [ ] **Step 3: Run** `make check-coreopts`. Expected: FAIL (no module).
- [ ] **Step 4: Implement** the module, the tables (each value's comment moves with it), the profile fields, `platform.c` over the module, the seeding change in `db.c`, the migration call in `main.c`.
- [ ] **Step 5: Run** `make check-coreopts check-db check-controls`, then the Step 1 comparison against this build: identical files for every tag, on a fresh card and on a card seeded as 1.0.5 then migrated. Expected: pass, no difference.
- [ ] **Step 6: Commit** "Core options: each device's defaults, at run time".

### Task 5: Diatom's power steps as each port's data

**Files:**
- Create: `diatom/port/port_power.h`
- Modify: `diatom/port/pixel2.c` (`diatom_port_power` and the GPU floor in `diatom_port_fast_clock` through its profile), `diatom/port/brick.c` (a comment: no profile until its clocks are measured; `diatom_port_power` stays a no-op), `diatom/test/port_test.c` or a new `diatom/test/power_test.c` with its rule in `diatom/Makefile`'s `check`

**Interfaces:**
- Produces (in `port_power.h`, inline, no SDL):
  - `typedef struct { const char *path; const char *at[3]; const char *top; } diatom_clock;` — a ceiling file and its value at FULL, MEDIUM and LIGHT (`DIATOM_POWER_*`), NULL to copy `top`'s file (the CPU's `cpuinfo_max_freq`) into it
  - `typedef struct { diatom_clock clock[4]; int n; const char *gpu_floor, *gpu_floor_top; } diatom_power_profile;`
  - `static inline bool diatom_power_set(const diatom_power_profile *p, int level);` — writes each clock in order, true when every write took
  - The Pixel 2's profile, in `pixel2.c`: CPU `DIATOM_CPUFREQ "scaling_max_freq"` {NULL, "1008000", "816000"} from `cpuinfo_max_freq`; `/sys/class/devfreq/dmc/max_freq` {"0", "0", "328000000"}; `/sys/class/devfreq/ff400000.gpu/max_freq` {"0", "300000000", "200000000"}; GPU floor `/sys/class/devfreq/ff400000.gpu/min_freq`, top "520000000"

- [ ] **Step 1: Write the test:** a profile of three clocks in a scratch folder; LIGHT writes each file's LIGHT value; FULL copies the `top` file into the first and writes the others' FULL values; a missing file makes it return false and still writes the rest.
- [ ] **Step 2: Run** `make -C diatom check`. Expected: FAIL.
- [ ] **Step 3: Implement** the header and the Pixel 2's profile; `diatom_port_power` keeps its "same level again does nothing" and its log line.
- [ ] **Step 4: Run** `make check-diatom` and the device compile (`make check-device-compile` in Ubuntu). Expected: pass.
- [ ] **Step 5: Commit** "Diatom: each port's power steps as data".

### Task 6: The record and the PR

**Files:**
- Modify: the spec (section 4 and section 9 step 3: built; section 9 step 4 gains `vendor/<arch>/`, `-mcpu` from the profile and the payload's cores per device; "Calls made" gets this plan's calls), `DESIGN.md` where a rule says a shelf exists on every device (the Brick's hidden GPU-frame shelves, the launch refusal's words)

- [ ] **Step 1: Update** the spec and DESIGN.md.
- [ ] **Step 2: Run** `make check`, `make pixel-gate BASE=origin/main GATE='--lang en'`, `make check-device-compile` in Ubuntu, a Pixel 2 build with plastron's SDK. Expected: pass, 0 changed.
- [ ] **Step 3: Shoot the sheet:** `--device brick` home and shelf list, the refusal under a Dreamcast game in Favorites, in light and dark, English and one more language; publish it as a page for the coordinator.
- [ ] **Step 4: Commit, fetch, push**, open the PR, bind it, tell the coordinator with the sheet's link.
