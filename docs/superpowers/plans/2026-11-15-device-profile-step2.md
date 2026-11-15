# Handheld as data (step 2) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The tutorial's and the Controls tab's 3D handheld drawn from a `td_model` that the device profile carries, so the Brick (step 5) is new data and a few new shape kinds, not a second copy of `src/tourdevice.c`; the Pixel 2 drawn pixel for pixel as today.

**Architecture:** Every millimetre constant in `src/tourdevice.c` (body, glass, front keys, side keys, lenses, shoulders, poses, kept boxes) moves into `const td_model td_model_pixel2` in `src/device/pixel2_model.c`. The drawing keeps its shape kinds as code (round key, d-pad, rocker pair, angled capsule, round key with dots, lens, top-edge shoulder) and draws the ones the model lists. Which way the handheld turns for a lesson, which keys a lesson lights, and which way the Controls tab leans follow from where the model puts each key and from the device's chords. `device_profile.handheld` becomes `const td_model *model`.

**Tech Stack:** C (gnu11), SDL2's `SDL_RenderGeometry` (unchanged), the host checks, the pixel gate.

**Spec:** `docs/superpowers/specs/2026-11-14-device-modular-design.md` (section 3, "The handheld as data"; section 9 step 2). Builds on step 1 (PR #172).

## Global Constraints

- Starts after PR #172 merges; branch `handheld-model` from the `main` that has it.
- The Pixel 2 does not change: `make pixel-gate BASE=origin/main GATE='--lang en'` reports 0 frames changed; `check-tour-shots`, `check-controls-shots` (with `--handheld-box tour` and `--handheld-box tab`) and `check-tour-colors` pass unchanged.
- Model data files include no SDL (`tdkey.h` and `profile.h` only), so SDL-free checks link them.
- The layout around the handheld (`TOURDEV_X`, `TOURDEV_Y`, `TOURDEV_SHIFT`, the Controls tab's column and `DEV_DX`) is the design system's (round 3); this step moves no position.
- Original art only: the model is geometry typed in millimetres; nothing is traced or loaded from an image.
- New C files start with the GPL-3.0-only SPDX line; commits and the PR in Tarik's first person, no AI mention.

## Calls this plan makes

- **Shape kinds stay code.** A fully generic mesh would be a rewrite with no gain: the Pixel 2's tabs, signs and hugging shoulders are tuned by hand. The model selects and places kinds; step 5 adds the Brick's (stepped body, back-ledge shoulders, front pills, round keys with an icon, separate side keys, slider). *Built:* the model holds the keys' faces, places and sizes, the views, the boxes and an `art` id; the body, glass, tabs, signs, rocker and lights stay the Pixel 2 art's own constants, which read the keys from the model (the spec's "Made while building step 2").
- **`td_key` grows F1 and F2 now** (the Brick's brightness keys), so lit-key logic is per device from this step; HOME, L3, R3 and the sticks come with the Brick Pro.
- **Button places come from the model**, not a second copy in the profile's button table: `dev_button_place()` reads the model.
- **The site's 3D model, the Blender script and the Mac simulator keep their own copies** (spec section 3).

## Review Focus

1. A lesson's turn on the Pixel 2: MENU and POWER turn right, the volume keys left, Brightness faces front with both side tabs, Move and SELECT tilt back. (Task 4 pins each lesson's derived pose to today's.)
2. The Controls tab's lean on the Pixel 2 for every row it can light. (Task 5 replays `ct_lean` for every key mask `ctlscr_box_check` uses.)
3. A pose's cache key: `td_pose` is compared as bytes; a model pointer inside it must not make two equal poses compare unequal or a stale texture survive a model change. (Task 2.)
4. A device whose profile has no model (the Brick until step 5): the tutorial keeps its block in the middle and the Controls tab column stays empty; nothing dereferences NULL. (Task 3.)
5. The kept textures' boxes: the Pixel 2's 304 x 382 (tutorial) and 256 x 332 (Controls tab) still hold every pose and turn. (Task 2, the existing `--handheld-box` checks.)

---

### Task 1: The model type and the Pixel 2's model

**Files:**
- Create: `src/device/td_model.h`, `src/device/pixel2_model.c`, `tools/model-check.c`
- Modify: `src/tdkey.h` (TD_F1, TD_F2 before TD_KEYS), `Makefile` (`check-model`, in `CHECKS`; the profile file list gains `src/device/pixel2_model.c`), `docs/building.md` (its row)

**Interfaces:**
- Produces, in `src/device/td_model.h`:
  - `typedef enum { TD_FACE_FRONT, TD_FACE_LEFT, TD_FACE_RIGHT, TD_FACE_TOP } td_face;`
  - `typedef enum { TD_SHAPE_ROUND, TD_SHAPE_DPAD, TD_SHAPE_PILL, TD_SHAPE_ROCKER, TD_SHAPE_CAPSULE, TD_SHAPE_DOTS, TD_SHAPE_SHOULDER } td_shape;`
  - `typedef struct { td_key key; td_face face; td_shape shape; float a, b, w, h, r, angle; } td_part;` — `a, b` the part's place on its face (front: x, z; side: y, z; top: x, depth), `w, h, r` its size, `angle` its turn on the face (the POWER capsule's -45 degrees)
  - `typedef struct { float deg, s, pitch; } td_view;`
  - `typedef struct td_model { const char *name; float body_w, body_h, body_r, front, back; float glass_w, glass_h, glass_r, glass_z; float screen_w, screen_h, screen_r, screen_z; float pocket_w, pocket_h, pocket_r, pocket_z; const td_part *parts; int n_parts; const float *lens_z; int n_lens; float lens_y; float sh_y, sh_base, sh_top; td_view front_view, right_view, left_view, both_view, top_view; int keep_tour_w, keep_tour_h, keep_tab_w, keep_tab_h; } td_model;`
  - `const td_part *td_part_of(const td_model *m, td_key k);` NULL where the model has no such key
  - `extern const td_model td_model_pixel2;`

- [ ] **Step 1: Write `tools/model-check.c`:** every value of `td_model_pixel2` equals today's constant in `src/tourdevice.c` (BODY_W 63.0, BODY_H 81.0, BODY_R 3.9, BODY_FRONT -8.5, BODY_BACK 8.5; FACE[] A (23.6, -21.5, 3.05), B (16.9, -28.2, 3.05), X (16.9, -14.8, 3.05), Y (10.2, -21.5, 3.05), SELECT (-3.8, -29.6, 2.85), START (4.0, -29.6, 2.85); DPAD (-16.9, -21.0); MENU right (-1.54, 17.13, r 2.65) with DOTS; POWER right capsule (-1.74, 6.65, 9.8 x 2.3, -45 degrees); four lenses at POWER's y (-1.74), z {1.41, -4.03, -9.67, -15.31}, 10.0 x 2.15 at -45 degrees; VOL rocker left (-1.82, 11.69) with VOL+ 16.52 and VOL- 7.05; shoulders L1 (-23.9, 15.1) R1 (23.9, 15.1) L2 (-12.6, 7.4) R2 (12.6, 7.4) on SH_Y 2.5, SH_BASE 36.0, SH_TOP 41.7; glass 55.1 x 43.8 r 0.8 at z 14.2; screen 48.8 x 36.6 r 0.3 at z 15.55; pocket 16.4 x 7.6 r 3.8 at z -29.6; views FRONT {10, 3.55, 0}, RIGHT {52, 3.95, 0}, LEFT {-52, 3.95, 0}, BOTH {0, 3.45, 0}, TOP {10, 3.55, TD_PITCH_DEG}; kept boxes 304 x 382 and 256 x 332); each key once; `td_part_of` finds each and NULL for TD_F1.
- [ ] **Step 2: Run** `make check-model`. Expected: compile error (no `td_model.h`).
- [ ] **Step 3: Write `td_model.h`, `pixel2_model.c`, `td_part_of`** (in `pixel2_model.c`'s neighbor `src/device/td_model.c`, SDL-free), and TD_F1/TD_F2 in `tdkey.h`.
- [ ] **Step 4: Run** `make check-model && make native && make check`. Expected: pass (nothing draws from the model yet; `td_pose`'s arrays grow by two keys, which are never lit).
- [ ] **Step 5: Commit** "Handheld: the Pixel 2 as a model".

### Task 2: Draw from the model

**Files:**
- Modify: `src/tourdevice.c` (every constant in "the model" and "the poses" sections, `pose_of`, the keep boxes in `tourdev_make`), `src/tourdevice.h` (`td_pose` gains `const td_model *model`), `src/ctltab_screen.c` (`KEEP_W/H` from the model), `tools/` nothing new

**Interfaces:**
- Consumes: Task 1's `td_model`, `td_part_of`.
- Produces: `td_pose.model` (set by every caller; `tourdev_pose` and `ctlscr`'s `pose_of` from the profile, Task 3); `draw_view` draws the parts the model lists through the existing shape helpers (`face_key`, `dpad`, `tab`, `vol_sign`, `shoulder`, `shoulders_tilted`, `side_rr`, `disc`, `oval`).

- [ ] **Step 1: Replace the constants** with reads from `v->model` (the `view` gets the model from `view_of`): body and glass from the model's fields; front keys by walking `parts` with `face == TD_FACE_FRONT`; the side keys and lenses by `face`; the shoulders by `TD_SHAPE_SHOULDER`. The order of drawing stays exactly today's (halos, shoulders, tabs, body, glass, front keys, side, rings), so the pixels do not move.
- [ ] **Step 2: The kept boxes** (`tourdev_make`'s 304 x 382, the Controls tab's 256 x 332) come from the model's `keep_*` fields.
- [ ] **Step 3: `td_pose` as a cache key:** the model pointer is compared with the rest of the bytes; `td_keep` also forgets its texture when `kept.model` differs (a profile switched on the host).
- [ ] **Step 4: Run** `make native && make check-tour-shots check-controls-shots check-tour-colors`. Expected: pass, including both `--handheld-box` measurements with no `HANDHELD-BOX:` line.
- [ ] **Step 5: Run** `make check` and the pixel gate. Expected: all pass, 0 frames changed.
- [ ] **Step 6: Commit** "Handheld: drawn from its model".

### Task 3: The profile carries its model

**Files:**
- Modify: `src/device/profile.h` (`bool handheld` → `const struct td_model *model`), `src/device/pixel2_profile.c` and `desktop_profile.c` (`.model = &td_model_pixel2`), `brick_profile.c` (`.model = NULL`), `src/profile.c` (`dev_tutorial_handheld` reads `p->model != NULL`), `src/ctltab_screen.c` (`if (!plat_profile()->model) return;`), `src/tourdevice.c` (`tourdev_pose` sets `td_pose.model` from the profile), `tools/devices-check.c`, `mk/cross.mk` (`$(wildcard src/device/$(DEVICE)_model.c) src/device/td_model.c`), `mk/native.mk` (all model files)

- [ ] **Step 1: Change `tools/devices-check.c`:** pixel2 and desktop have `model == &td_model_pixel2`, brick `model == NULL`.
- [ ] **Step 2: Run** `make check-devices`. Expected: fails (no `model` field).
- [ ] **Step 3: Change the profile and its readers** as listed. The Brick's device build links no model file.
- [ ] **Step 4: Run** `make check`, the pixel gate, and the Pixel 2 SDK build (`build/px2/zyperos.elf` links). Expected: pass, 0 changed.
- [ ] **Step 5: Commit** "Devices: the profile carries its handheld's model".

### Task 4: Poses and lit keys from the model and the chords

**Files:**
- Modify: `src/tourdevice.c` (`pose_of`, `lit_of`), `tools/tour-shot-check.py`

**Interfaces:**
- Produces: `static unsigned lit_of(const tour *t, tour_step s)` — Move: the d-pad and L1/R1; SELECT: SELECT and L2/R2; Brightness: MENU and the volume keys where `t->bright == DEV_BRIGHT_MENU_VOL`, F1 and F2 where it is `DEV_BRIGHT_KEYS`; the rest as today. `static td_view pose_of(const td_model *m, unsigned lit)` — any lit `TD_SHAPE_SHOULDER` → `top_view`; lit keys on both side faces → `both_view`; all on the right face → `right_view`; all on the left → `left_view`; otherwise `front_view`.

- [ ] **Step 1: Add a named case to `tools/tour-shot-check.py`:** in the Brightness lesson with `--fn-volume`, both side tabs stand out (lime on the left and on the right of the body's column box); in the MENU lesson, lime only on the right half of the column. (The pixel gate holds every other lesson's frame.)
- [ ] **Step 2: Implement** the two functions.
- [ ] **Step 3: Run** `make check-tour-shots` and the pixel gate. Expected: pass, 0 changed.
- [ ] **Step 4: Commit** "Handheld: a lesson's turn and lit keys from where its keys are".

### Task 5: The Controls tab's lean from the model

**Files:**
- Modify: `src/ctltab.c` (`ct_lean(unsigned keys, float *yaw, float *pitch)` → `ct_lean(const td_model *m, unsigned keys, float *yaw, float *pitch)`), its callers in `src/ctltab_screen.c`, `tools/ctltab-check.c` or the existing remap/ctltab check that covers `ct_lean`

**Interfaces:**
- Produces: a key leans left when its part's place is left of the middle on the front or top face (`a < 0`), right when right of it; the shoulders tilt (`CT_PITCH_DEG`); the d-pad's own lean `CT_LEAN_DPAD_DEG` where the d-pad alone is lit, as today.

- [ ] **Step 1: Write the check:** for every key mask `ctlscr_box_check` lists, `ct_lean(&td_model_pixel2, mask, ...)` equals today's `ct_lean(mask, ...)` (compute today's values once into the check as a table).
- [ ] **Step 2: Run it.** Expected: fails until Step 3 (signature).
- [ ] **Step 3: Implement**, the callers passing `plat_profile()->model`.
- [ ] **Step 4: Run** `make check-controls-shots && make check` and the pixel gate. Expected: pass, 0 changed.
- [ ] **Step 5: Commit** "Controls tab: the lean from where the keys are".

### Task 6: Button places for the design system, and the record

**Files:**
- Modify: `src/profile.c` and `src/device/profile.h` (`bool dev_button_place(const device_profile *p, in_button b, td_face *face, float *x, float *z);` through an `in_button` → `td_key` map; `profile.h` includes `td_model.h`, both SDL-free), `tools/devices-check.c`, the spec's "Calls made" (this plan's calls; step 2 built), `DESIGN.md` 11.10 and 11.45 where they say "the Pixel 2's handheld" for the drawing code (now "the device's model")

- [ ] **Step 1: Write the cases:** the Pixel 2's A is on the front at (23.6, -21.5); MENU on the right at (-1.54, 17.13); IN_BRIGHTUP has no place; the Brick has none (no model).
- [ ] **Step 2: Implement, run** `make check-devices`. Expected: pass.
- [ ] **Step 3: Update the spec and DESIGN.md; run** `make check` and the pixel gate. Expected: pass, 0 changed.
- [ ] **Step 4: Commit, push** (after a fetch), open the PR, tell the coordinator when CI is green.
