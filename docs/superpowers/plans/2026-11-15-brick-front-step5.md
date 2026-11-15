# The Brick's front end (step 5) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The TrimUI Brick looks right on the Mac (`--device brick`): its own 3D handheld in the tutorial and the in-game Controls tab, its MENU key as printed, its key hints fitting, every Brick screen on a contact sheet for Tarik; the README and i18n shots wear `--device pixel2`; the panel probe kit is ready for testers.

**Architecture:** The Brick gets a `td_model` (`src/device/brick_model.c`) and its own art (`TD_ART_BRICK` in `src/tourdevice.c`, beside the Pixel 2's), reading its keys from the model as the Pixel 2's art does since step 2. Its shoulders sit on the back ledge, which no front view shows, so the model gains a face for them (`TD_FACE_LEDGE`) and a rear view (`back`) that lessons and Controls rows lighting them turn to. The profile points at the model; `dev_tutorial_model` drops its MENU+VOL condition once the host shots stop borrowing the Pixel 2 (the README and i18n shots wear `--device pixel2`).

**Spec:** `docs/superpowers/specs/2026-11-14-device-modular-design.md` (section 6, "The Brick's front end"; the appendix "Measurements"; section 8, testing; section 9 step 5).

## Global Constraints

- Original art only: the model is geometry from the published outline (73.2 x 109.9 mm; 11.8 deep above the ledge, 19.9 below) and the appendix's measured key places; TrimUI's reference views (in the coordinator's notes folder, `brick-reference/`) are for proportions and layout, never shipped or traced; no TrimUI mark anywhere (the MENU key shows the word MENU, the d-pad's centre is plain).
- The Pixel 2 does not change: `make pixel-gate BASE=origin/main GATE='--lang en'` 0 frames changed, except the shots this step moves to `--device pixel2` on purpose, which go on their own sheet with their before/after.
- Every Brick screen on a contact sheet for Tarik before the PR is called done.

## Facts for the model (millimetres; model x right and z up from the body's middle; front-view x from the left edge and y from the top become x - 36.6, 54.95 - y)

- Body 73.2 x 109.9; front corners top r 0.5, bottom r 4.1; thin part 11.8 deep, full height; thick part 19.9 deep from y 42.9 (z 12.05) down; the ledge is the thick part's top face, z 12.05, from depth 11.8 to 19.9.
- Glass x 0.9 to 72.2, y 0.8 to 57.2; active screen 65.0 x 48.8 centred in it.
- D-pad centre (15.9, 72.3) -> (-20.7, -17.35), span 19.3, arm 7.0 wide.
- F1 (30.9, 63.6) -> (-5.7, -8.65), F2 (41.6, 63.6) -> (5.0, -8.65); pills 7.1 x 2.6.
- Face diamond centre (56.3, 72.1) -> (19.7, -17.15); keys 7.6 from it, 7.7 across: A right, B bottom, X top, Y left.
- Bottom round keys at y 101.4 (z -46.45), 5.4 across: MENU (26.1 -> x -10.5, the vendor's mark there, the word here), SELECT (36.1 -> -0.5, a square), START (46.2 -> 9.6, a play triangle). Read from the reference's front view, 2026-11-15.
- Speaker grilles (11.1, 99.6) and (61.9, 99.6): 7.4 wide, two rows of dots.
- Left side: VOL+ at y 10.3 (z 44.65), VOL- at y 21.4 (z 33.55), each 7.7 long, on the thin part.
- Right side: the FN slider at y 23.3 (z 31.65), 10.3 long (no key of the launcher's: drawn, never lit); POWER at y 39.3 (z 15.65), 7.1 across.
- Shoulders on the ledge, left to right: L1 (~17.5 wide, centre x -27.9), L2 (~11 wide, x -12.3), the USB-C housing (~13.5 wide, centred), R2 (x 12.3), R1 (x 27.9); keys from y 41.3 to 47.0 (they stand up to z 13.65 off the ledge).
- Top edge: a light bar 37.9 wide, centred, at the back. Bottom edge: card slot, a hole, USB-C, a pinhole, the 3.5 mm jack.

## Views (decided here; on the contact sheet for Tarik)

- `front`, `right`, `left` as the Pixel 2's (turned to the side its keys are on).
- `back`: turned around (about 160 degrees, the right side's way) and tilted back about 20 degrees, so the ledge and its four shoulders show. The ledge is hidden from any front view tilted less than about 80 degrees (the thin part stands in front of it), so a lesson or a Controls row that lights a shoulder turns to the back. Move Around (d-pad and L1/R1) shows the back: the shoulders are the keys a new player looks for; the d-pad shows in the lessons before.
- No `both` (the Brick's brightness keys are F1 and F2 on the front) and no `top`.

## Tasks

1. **The panel probe kit** (`plastron/scripts/build-brick-probe.sh` falls back to the Pixel 2 build's compiler, mkimage and genimage; `make brick-kernel` then `make brick-probe`; its README is `board/brick/probe/README.txt`). Its own PR. Then tell the coordinator what testers do.
2. **The model and the ledge face**: `TD_FACE_LEDGE`, `td_view back`, `src/device/brick_model.c`, `check-model` cases, `view_for` and `ct_lean` treating a ledge key as a shoulder on the back.
3. **The Brick's art** in `src/tourdevice.c` (`TD_ART_BRICK`): the two slabs, the front, the sides, the ledge and shoulders, from any turn; kept boxes measured with `--handheld-box` under `--device brick`.
4. **The profile wears it**, `dev_tutorial_model` drops the MENU+VOL condition, the host shots that show the Pixel 2's chords wear `--device pixel2` (README and i18n shots, their own sheet), the desktop profile stops borrowing the Pixel 2's model.
5. **MENU, hints, words**: the MENU glyph from the profile's label and cap everywhere a key is drawn; every fit check and shot also under `--device brick`; the About credit names each device's kernel.
6. **The contact sheet** of every Brick screen (both themes, English and one more language), published for Tarik; the PR(s).
