# ZyperOS on more than one handheld: a device profile, then the TrimUI Brick and Brick Pro

## Intent

ZyperOS runs on one handheld today, the GKD Pixel 2. For 1.0.6 I want it
ready for other handhelds, possibly with other CPUs, and then I want the
TrimUI Brick to be the second one and the TrimUI Brick Pro the third. ZyperOS grew out of TortOS, which ran on
the Brick, so much of the Brick's code is still in the tree: a device file
for the launcher, a Diatom port, a card script, a whole plastron board. But
every feature since early November was built and tried on the Pixel 2 alone,
and the Brick's side was kept compiling with stubs, or not at all.

"Ready" means: everything in the system that is about the handheld, from
the card layout to the tutorial's 3D drawing, is declared in one place per
device, and the rest of ZyperOS asks that place what the handheld has, never
which handheld it is. A new handheld is then a profile, a few drivers and
its art, not a hunt through the tree for the places that assumed a Pixel 2.

The Brick is the test of that: if adding it is mostly filling in its
profile, drawing it, and catching its drivers up, the layer is right.

## Goals

- **One device profile per handheld**, a declaration of what it has: the
  screen, the buttons and how they are printed, its 3D drawing, lights,
  battery, USB, radios, GPU, CPU, sound, card layout and updates.
- **Capabilities, not identities.** No screen, feature or script decides
  anything from "is this a Pixel 2". Every place that does it today, in
  code, by a file being present, or by a stub's answer, asks the profile.
- **The Pixel 2 unchanged.** Every Pixel 2 screen is pixel-identical before
  and after the refactor (the pixel gate), and its speed is the same.
- **Every device can be looked at on the Mac.** The host build wears any
  profile (`--device pixel2`, `--device brick`), so the tutorial, the
  Controls tab, MENU and every shot check run for each handheld without one.
- **Every device file is compiled by `make check`**, so a device nobody is
  holding stops rotting.
- **The Brick as the second device**: its own tutorial handheld and Controls
  tab handheld (original art), its key hints and button glyphs, its systems
  and cores, its image, installer support and release artifacts.
- **The Brick Pro as the third device**: the Brick's chip in a bigger body
  with a bigger screen, and the first ZyperOS handheld with analog sticks.
- **Room for another CPU.** The core set, the build flags and the release
  carry an architecture, even while every device is aarch64.

## Non-goals

- **One image for every device.** Each handheld keeps its own image, built
  from its own plastron board. The kernels, boot loaders and GPU drivers
  differ anyway.
- **A screen that is not 4:3.** The launcher keeps drawing 1024x768. A 16:9
  device is a later design; the profile carries the aspect so such a device
  can say it does not fit yet.
- **Touch screens.** None of the three devices has one. (Analog sticks and
  L3/R3 are not a non-goal any more: the Brick Pro has them, section 6b.)
- **Building cores for armhf or x86_64 now.** The architecture becomes a
  name in the paths and the release; only aarch64 is filled in.
- **The Brick Pro in 1.0.6.** It comes after the Brick (section 6b, steps 8
  and 9). The RK3566 handhelds and other chips are what the profile should
  be able to take later; none is added here.
- **Renaming the `px2-*` debug tools.** They gain a device; their names stay.

## Anti-goals

- **A Pixel 2 that gets worse.** No dropped frame, no slower start, no
  changed pixel, no lost feature, because of the profile.
- **A device switch in the UI.** No `if (brick)` in a screen. A screen reads
  a capability or a piece of the profile's art, and draws.
- **A Brick that offers what it cannot do.** No GPU shelf that fails to
  load, no Player 2 that never connects, no lesson for side lights it does
  not have, no System Update row that cannot update.
- **Somebody else's art.** The Brick's handheld is drawn in code, as the
  Pixel 2's was: its own geometry, built from measurements. Published
  dimensions and the maker's own views give the numbers; nothing is traced
  or copied from other people's renders, 3D models, icon packs or photos.
- **Closed files in git.** The Brick's GPU libraries and Wi-Fi firmware stay
  outside the repository, as today.

## Constraints

- **I have no Brick.** People on the ZyperOS Discord have one and offered
  to help and test. Everything up to the Brick's image is built and checked
  on the Mac with `--device brick`; the image, the drivers and the GPU cores
  are tried by those remote testers (section 8).
- **Closed inputs.** A Brick image needs Imagination's PowerVR DDK 1.11
  userspace for the GE8300 (`BRICK_GPU`) and XRadio's XR829 firmware
  (`BRICK_XR829`), neither in the repository nor on this Mac
  (`~/brick-runtime` does not exist), plus the Allwinner 4.9 kernel base
  tarball that `make brick-kernel` downloads.
- **Two refactors run first.** The main.c split round 2 (`main-split-2`) and
  the design system round 2 (`design-system-2`) touch `src/main.c`, the
  panels and the tutorial's screen code (`tour_screen.c`, `xfer_screen.c`,
  `usbdrive_screen.c` move out of main.c there). This work writes no launcher
  code until both are merged.
- **The design system's round 3** brings a layout engine (Clay, Yoga or its
  own) and resolution-independent tokens on a 4-unit grid. The seams with it
  are agreed (2026-11-15) and go into its round 3 spec too:
  - **Screen.** The profile gives physical facts only: panel pixels, turn,
    active area in millimetres, insets in panel pixels, aspect, whether a
    panel still can be saved. The layout system computes the canvas from
    them: 768 units high, 768 x aspect wide on the 4-unit grid (1024 on all
    three devices). The profile states no scale: `plat_present` derives it
    from `panel_h / 768`. The layout converts insets to canvas units,
    rounds them up to the grid and adds them to its safe-area token (all 0
    today). Round 3 uses no density; the millimetres stay in the profile
    for a later density rule, which is mine to decide (the same canvas is
    0.048 mm a unit on the Pixel 2, 0.063 mm on the Brick, 0.078 mm on the
    Brick Pro). The platform layer stays the only code that knows the turn.
  - **Buttons.** See section 1, "Buttons differ from device to device".
- **Performance first** (the Pixel 2's frame budget): a profile is constant
  data read at start; nothing is looked up per frame that was a literal.
- **The license.** ZyperOS is GPL-3.0-only; the Brick code from the TortOS
  days is GPL-3.0 too and keeps its notices.

## What exists today

The full map, with file and line for each item, is in the appendix. The
short of it:

- **The launcher already has a seam.** `src/platform.h` is the only way the
  rest of the launcher reaches hardware; `src/device/<name>.c` behind it is
  picked by `DEVICE=` at build time (`mk/cross.mk:6`, `mk/native.mk:10`, both
  default `brick`); `src/device.h` is the seam inside. `pixel2.c` and
  `brick.c` define the same 64 functions. The launcher draws a 1024x768
  landscape canvas everywhere, which is the Brick's panel; the Pixel 2 scales
  and turns it to its 640x480.
- **Diatom has the same shape.** `diatom/include/diatom_port.h` (37
  functions and a caps struct), ports `pixel2.c`, `brick.c`, `desktop.c`,
  `PORT ?= desktop`. `brick.c` defines all 37, but nothing compiles it: the
  GPU frames, the power profiles, the GPU shaders and rumble are stubs or
  fall back there.
- **The device is guessed, not declared.** plastron picks the launcher's and
  Diatom's device from whether the PowerVR package is in the build
  (`package/zyperos/zyperos.mk:45`, `package/diatom/diatom.mk:11`,
  `package/splash`, `external.mk`). The launcher learns its device id from
  the basename of `P_USERDATA` (update names, the web page's `device`).
  Features are found by side effects: an unknown USB peer means "no shared
  port" (`main.c:4309`), a missing `/usr/sbin/usbdrive` hides Flash Drive, a
  missing `zyperos-update` hides System Update, `hostapd` present means a
  hotspot.
- **One flag stands in for "this is a Pixel 2".** `fn_volume`, from
  `plat_brightness_on_volume()`, decides the brightness chord, MENU acting on
  release, whether the tutorial draws a handheld, and the MENU+START Home row
  (which is really zyhome, a Pixel-only helper): four capabilities in one bool,
  in `tour.c`, `tourdraw.c`, `tourdevice.c` and `controls.c`.
- **The handheld is the Pixel 2's, hard-wired.** `src/tourdevice.c` holds
  the 63 x 81 x 17 mm body, every key's place, the poses and which side each
  key is on, copied by hand from `tools/build-pixel2-model.py`; `td_key` is
  exactly the Pixel 2's keys. The tutorial hides it on the Brick by design;
  the in-game Controls tab does not, so a Brick would show a Pixel 2 there.
- **The MENU glyph is the Pixel 2's key everywhere** (`ui.c`): every word
  "MENU" in running text is drawn as the round three-dot key.
- **The Brick's code has rotted in places.** Its card script has not changed
  since 1.0.0 (no ports, no `run:` programs, no PPSSPP assets, no reboot or
  restart flags). `brick.c` lacks the charger-online fix that stopped Auto
  Off powering down a plugged-in Pixel. The About page's Developer Mode
  unlock reads L2+R2 through `plat_tap_held`, which is always false on the
  Brick, so it can never unlock there. `zyhome` waits for the Pixel's MENU
  code. plastron's Brick build is expected to stop at `px2-shot`'s GETFB2
  (newer than the Brick's 4.9 kernel headers; not tried), and its release
  stops at `third-party-packages.md` (no xz). The Brick card has one root, a
  GPT with the games partition at p4, where `update/slots.c` expects slot B.
- **The cores do not say what they need.** Nothing in `config/systems.cfg`
  says that Dreamcast, NAOMI, Atomiswave, N64, PSP and Saturn need GPU
  frames; the Brick's Diatom refuses them at SET_HW_RENDER, but the launcher
  still shows the shelves. The cores are one flat aarch64 set with no
  architecture in their paths, and the Pixel's tuned core options seed the
  shared `library.db`.
- **The release is the Pixel 2's.** `tools/release.sh` builds and names
  only `pixel2`; CI checks the `pixel2` image; the site's download page and
  `downloads.js` know one image. The installer already knows both devices
  (`PX2BOOT` / `BRICKBOOT` labels, `ZyperOS-v<ver>-{brick,pixel2}.img.xz`), but
  as one `brick: bool`, and its Brick update writes one root at p2.

## Approaches

**A. One profile per device, chosen at build time, with drivers beside it
(my pick).** A device is two things: a declaration (`src/device/<id>.profile.c`:
constant data, no SDL, no hardware) and its drivers (`src/device/<id>.c`
and `diatom/port/<id>.c`, as today). The device build links one profile; the
host build links every profile and picks one with `--device`. plastron names
the device in the defconfig instead of guessing it. Everything that asked
"which device" asks the profile. It keeps the seam that already works, costs
nothing per frame, and gives the Mac every device's screens.

**B. One launcher binary for every device, finding its device at run time**
(from the device tree's `compatible`). It would let one payload serve two
aarch64 handhelds. But the images differ anyway (kernel, GPU driver, boot
loader, Diatom port), so one binary buys nothing a user sees, and it puts
every device's drivers in every build, where a probe that guesses wrong is a
handheld that does not start.

**C. A profile as a text file on the card or in the image**, read at start.
Community devices could be added without C. But most of a device is code
anyway (key codes, sysfs paths, the display), the shot checks and the pixel
gate want the profile in the build, and a wrong file on a card is a support
case. It can come later on top of A, for the parts that are pure data.

I go with **A**.

## The design

### 1. The device profile

A profile is a `const device_profile` in `src/device/<id>.profile.c`, read
through `plat_profile()`. It holds only facts about the handheld. Every
field has a name a screen can read without knowing the device. The same data
reaches the places that are not the launcher (Diatom, portkeys, zyhome,
plastron, the payload, the release, the installer) through one generated
file per device, `build/device/<id>.mk` and `<id>.h`, written from the
profile by a host tool, so a value is stated once.

| Part | What a device declares |
|---|---|
| **Identity** | `id` (`pixel2`, `brick`), the name people use ("GKD Pixel 2", "TrimUI Brick"), the `.userdata/<id>` folder, the host name, the boot partition's label (`PX2BOOT`, `BRICKBOOT`), the image's name part |
| **Screen** | panel width and height in pixels, its turn (0, 90, 270), the active area in millimetres, insets in panel pixels (0 on all three), the aspect (4:3 only for now), whether a still of the panel can be saved for a port's loading screen. No canvas size and no scale: the layout system derives the canvas, `plat_present` the scale |
| **Buttons** | which of `in_button` exist; how each is printed on the device (label and shape: letter, word, round with dots, pill, rocker); the face layout (SNES: A right, B bottom); MENU as a real button or as FUNCTION reported on release; L2/R2 as plain buttons or as taps; the chords: brightness (own keys, or MENU+VOL), the battery level (L2+R2 on lights, or none), Home (MENU+START, or none), the Developer Mode unlock |
| **Handheld art** | a `td_model`: body size and corners, the body's color roles, every key's face (front, left, right, top), place and shape in millimetres, the lights' lenses, the poses (turn and tilt per face), the drawing's scale and column on the tutorial and the Controls tab, the kept texture's box. `NULL` for a device that has none yet, and every screen copes |
| **Lights** | none, or side lights: how many, which parts (battery, levels, files, cheer), the FIFO line format already used by `gauge` |
| **Battery** | the gauge's power-supply names, whether the charger can stop short of full (charge limit), the warning temperature, the empty-battery rules that are the PMIC's (hold POWER for 10 s) |
| **USB** | gadget link (interface, address, UDC, gadget name), host role (needs a role switch), power out, phone tethering, mass storage (Flash Drive), whether the port is shared between the computer and accessories |
| **Radios** | Wi-Fi built in, Wi-Fi by adapter, or none; Bluetooth; a hotspot (Player 2's) |
| **GPU** | none, or the GLES version and the GPU-frame path Diatom can use; whether the old-screen shaders run on it; a GPU clock to hold |
| **CPU** | architecture (`aarch64`), which picks the shared core set and the build baseline (`-mcpu=cortex-a53` for every aarch64 device, as today), the real core for the record (`cortex-a35`, `cortex-a53`), cores in its cluster, the clock and memory steps for light and medium power |
| **Sound** | one speaker or two, a mute switch, the level ladders (the codec's controls, the attenuation windows, the backlight's rungs), in one `<id>_levels.h` shared by the launcher, Diatom's port and portkeys, as `pixel2_levels.h` is today |
| **Card and updates** | the partition table (MBR, GPT), boot, root slots and games partition numbers, slot size, whether System Update can switch slots, the boot screen's theme |
| **Payload** | which helpers ship (p2d, zyhome, portkeys, btplayer, usbdrive), whether ports and `run:` programs run (PortMaster, OpenBOR), the PortMaster device block |

**Buttons differ from device to device** (Tarik, 2026-11-14), so the
profile's buttons part is a table, one row per physical control: its
`in_button` id (or a new one: HOME, L3, R3, the sticks), whether this
device has it, the label printed on it ("A", "MENU", "F1", a shape), its
cap's shape (round, pill, stick) with an icon from the design system's
closed set (square, play triangle, three dots, stick, none), its cap color,
its place on the handheld model, and its roles in chords (MENU or FN with
the volume keys, HOME, MENU with START, SELECT with the shoulders for
Diatom's hotkeys). Agreed with the design system (2026-11-15):

- **One glyph component** in `src/ds/` draws any button from its label and
  cap shape and icon, at sizes on the 4-unit grid. The design system owns
  it and the glyph families.
- **Hints, key rows and chords name buttons by id**, never by a printed
  string. A chord is a list of ids, where "+" means together (SELECT +
  START) and "/" means either (L1 / R1); today's one "L1/R1" pill becomes
  two glyphs.
- **On-screen glyphs use the theme's colors**, so contrast holds. The cap
  color is for the 3D handheld only (the Brick Pro's maroon A B X Y).
- **A vendor logo is never an icon.** A key that shows TrimUI's mark (the
  Brick's MENU) gets its word, "MENU", as its label, and the glyph draws
  that word. The Pixel 2's MENU keeps the three dots, an icon of the set.
- **Fit runs per device**, with the keys-overlap check. The full i18n set
  runs on the default shot device; on the others, the screens whose fit
  depends on glyph widths, unless all three fit the check budget (the
  design system decides from measured times).

Every key hint, tutorial lesson, Controls row, chord help line and
wrong-press answer reads its buttons from this table; none names a Pixel 2
key. The fields can land in step 1; the design system's round 3 migration
is the first to read them.

`plat_*` functions that were capability questions (`plat_has_wifi`,
`plat_gpu_shaders`, `plat_speaker_mono`, `plat_brightness_on_volume`,
`plat_battery_lights`, `plat_player2`, `plat_charge_limit_supported`) become
profile fields; the ones that read live state (`plat_menu_held`,
`plat_tap_held`, `plat_usb_peer`, the battery) stay functions. `fn_volume`
splits into the four capabilities it stood for.

The host build runs a `desktop` driver (split out of `brick.c`, which then
is the Brick's alone) and any profile. Today's host build answers as neither
device: mostly as a Brick (Bluetooth, two speakers, F1/F2, no side lights,
no Player 2), and as a Pixel 2 for the GPU shaders when headless. So the
default host profile, `desktop`, copies exactly those answers, and every
shot made today comes out the same after step 1 (the pixel gate stays
clean). Shots of a real device run with `--device pixel2` or
`--device brick`; `--fn-volume` becomes an alias of `--device pixel2` for
the tutorial and Controls checks that use it. Moving the README and i18n
shots to `--device pixel2` changes pictures, so it is its own step with a
reviewed sheet (step 5). *(My call: the refactor PRs prove themselves with
an unchanged gate; changed pictures come only where they are meant.)*

**What the hardware has, and what the system has.** The profile says what
the handheld can do. Whether the system on this card has a part
(`usbdrive`, `zyperos-update`, `hostapd`, `p2d`) is a second fact, because a
card can hold a newer ZyperOS folder on an older system (`res/update/min-system`
is 1.0.3; Flash Drive needs 1.0.4's). The image writes the parts it has into
`/etc/zyperos/system` (a `has` line), and `plat_system_has(part)` reads it,
falling back to today's file checks on a system from before that line. A
feature shows when the profile allows it and the system has it. *(My call:
no feature that fails on an older system, and no `min-system` raise.)*

### 2. Checks that keep it honest

- **`make check-devices`**: every profile fills every field; every
  generated file is current; no `pixel2`, `brick`, `PIXEL2` literal or
  `DEVICE` test in `src/` outside `src/device/`; no capability is read by a
  side effect (`access("/usr/sbin/usbdrive")` outside `plat_system_has`,
  `plat_usb_peer() != USB_PEER_UNKNOWN` as a capability).
- **Every device file compiles**: a Linux job in CI (the repository is
  public, so its minutes are free) compiles the Linux halves of
  `src/device/*.c` and `diatom/port/*.c` with `-fsyntax-only` against the
  runner's own kernel and SDL headers; that code is evdev, sysfs and ioctls,
  not CPU-specific. On the Mac, `make check-devices` does the same in the
  Docker SDK when it is there and prints SKIP when it is not. A missing stub,
  a changed signature or a renamed function shows up without a Brick build.
  What only the Brick's older 4.9 kernel headers show (as GETFB2) still needs
  a Brick build. *(My call: catches most of the rot the map found, at no
  device cost.)*
- **Shot checks for each device**: the tutorial, Controls tab, MENU >
  Controls, empty home and File Transfer shots run once per profile.
- **The pixel gate** (`make pixel-gate`) on every refactor PR: the Pixel 2's
  shots before and after are the same.

### 3. The handheld as data

`src/tourdevice.c`'s geometry moves into a `td_model`: the Pixel 2's numbers
become `src/device/pixel2.model.c`, drawn exactly as today (pixel gate). The
drawing code walks the model's faces and keys instead of naming the Pixel 2's.
`td_key` grows to the union of the devices' keys (F1 and F2 for the Brick's
brightness keys). Which way the handheld turns for a key, in the tutorial,
and which way it leans on the Controls tab (`ct_lean`), follow from the key's
face and place in the model, not from a table of the Pixel 2's sides. The
tutorial shows the handheld when the profile has a model, not when
brightness is MENU+VOL; the Controls tab shows it the same way, and a device
with no model gets the panel without the column.

The website's 3D model, the Blender script and the Mac simulator's frame keep
their own copies for now; one source for all three is later work.

### 4. Systems, cores and power by capability

- **`config/systems.cfg` gets a `needs|TAG|gpu-frames` line** for the systems
  whose cores ask for GPU frames (Dreamcast, NAOMI, Atomiswave, N64, PSP,
  Saturn). A device without them does not show those shelves and its payload
  does not carry those cores; a game moved over from another card says why
  it will not start, in the words other missing pieces use.
- **The power steps move into the Diatom port's profile** (CPU, memory and
  GPU ceilings per step, the paths to set them); `systems.cfg` keeps the
  words `light` and `medium`, which each device maps to its own clocks.
- **Core option defaults per device.** The defaults in `src/db.c` that were
  tuned for the Pixel 2's Panfrost become the `pixel2` profile's, read at run
  time and no longer seeded into the shared `library.db`; a device takes its
  own. A row in `library.db` is then only the player's own choice, and it
  holds on every device, as a choice for that game should. A one-time step
  deletes the seeded rows that still equal a Pixel 2 default (the profile
  gives them back on the Pixel 2), so a card moved to another handheld does
  not carry the Pixel's tuning. *(My call: no user setting lost, one small
  migration.)*
- **The cores get an architecture.** `vendor/cores` becomes
  `vendor/<arch>/cores`, and `mk/fetch-vendor.sh`, `mk/core-stamp.sh`,
  `mk/payload.sh` and the cores release carry the architecture in their
  names. Only `aarch64` exists. One core set per architecture, built for the
  architecture's baseline (`-mcpu=cortex-a53`, which both devices run; PPSSPP
  stays at `cortex-a35`, which runs on the Brick's A53 too). The Pixel 2's
  payload stays byte for byte the same. *(My call: a core set per device would
  double the cores release for no measured gain.)*

### 5. Build, card and release by device

- **plastron names the device.** Each defconfig sets
  `BR2_PACKAGE_ZYPEROS_DEVICE="pixel2"` (or `brick`, or `pixel2` for QEMU);
  `zyperos.mk`, `diatom.mk`, `splash.mk` and `external.mk` read it instead of
  testing for PowerVR. The payload's helpers follow the profile's payload
  part.
- **One card script.** `sd/pixel2/launch.sh` and `sd/brick/launch.sh`
  become one `sd/zyperos/launch.sh` with a short `device.sh` per device
  (its environment, radios, backlight helper). The Brick then gets ports,
  `run:` programs, the reboot and restart flags, the loading still and
  everything the Pixel's script gained since 1.0.0.
- **System Update reads its device and slots from the image.**
  `/etc/zyperos/system` (written at image build from the profile) carries the
  device id and the boot, slot and games partition numbers; `update/slots.c`
  and `update/apply.sh` read them instead of p1/p2/p4 and a `pixel2` default.
- **The release builds a list of devices.** `tools/release.sh` builds, names,
  signs and checks every device in `config/devices`. SHA256SUMS, the update
  lists (`ZyperOS-<id>-update.txt`), `res/update/min-system` (per device) and
  CI's release check cover each one. Once a device has been released, every
  later release carries it: System Update and the installer read
  `releases/latest`, so a release without a device would leave that device
  with nothing to find. A release waits until each listed device passed its
  device test for that version; the pass is a `Tested on` line per device in
  `docs/releases/v<ver>.md`, which `tools/release.sh` reads and refuses to
  publish without. A device is added to `config/devices` with its first
  release. *(My call: one rule, no carried-forward files, no updater that
  searches old releases. If a device ever cannot keep up, this is the rule to
  revisit.)*
- **The installer lists devices from the release**: a device with no image
  in the newest release is not offered (today it offers the Brick and fails
  at download). Its Brick update path follows the profile's card layout.
- **Site and docs**: the download page lists one button per released device;
  the README and guide say which features each device has, from the same
  capability list.
- **Dev tools**: `tools/px2` and `px2-deploy.sh` take the device (address,
  key, log folder, lock); `px2-shot` and `px2-lat` get a framebuffer path for
  the Brick, whose display is fbdev, not KMS.

### 6. The TrimUI Brick

The Brick's profile, as far as the code knows it (to be checked on the
device):

- **Screen**: 1024x768, no turn, scale 1: the canvas is its panel.
- **Buttons**: on the front, an 8-way d-pad, the A B X Y diamond, two lit
  pill keys above the face buttons (F1 and F2: brightness) and three round
  keys along the bottom (MENU, SELECT and START: TrimUI's mark, a square and
  a play triangle on the caps; on screen MENU is the word, as no vendor
  logo is an icon; which cap is which is checked against the key codes). L1 L2 R2 R1 sit on a ledge on
  the back, where the thin upper half meets the thick lower half, with a
  USB-C port between L2 and R2 (L2 and R2 are plain buttons; the shoulder
  caps come off, in a rectangular and a rounded set). VOL+ and VOL- on the
  left side; on the right side POWER and a slider that TrimUI sells as a
  "custom FN switch" and `brick.c` reads as a mute switch (gpio243). MENU
  is a real button (`BTN_MODE`).
- **Radios**: Wi-Fi and Bluetooth built in (XR829). A hotspot for Player 2:
  off until it is tried on the device.
- **USB**: two USB-C ports. The bottom one charges and is the gadget link
  to a computer (10.55.0.2, NCM). The one on the back ledge is a USB host
  for game controllers in TrimUI's system; plastron's Brick kernel has USB host off
  (kernel patch 0019), so controllers on the top port are later work. No
  power out to a phone, no phone tethering, no Flash Drive at first.
- **Battery**: AXP2202, with the charger-online rule the Pixel 2 needed; no
  charge limit until the driver has one.
- **GPU**: PowerVR GE8300 through Imagination's closed GLES; GPU frames
  **off** until a GPU core runs on it (Diatom's fbdev EGL path is new work);
  the old-screen shaders stay the CPU's Sharp map, as today. A static read
  of the exact DDK 1.11 files plastron pins (their SHA-256 match
  `files.sha256`; read with REA and Ghidra, never run) found what that path
  needs:
  - EGL 1.4 ("1.4 build 1.11@5516664"), OpenGL ES only, with
    `EGL_KHR_fence_sync`, `EGL_KHR_wait_sync`, `EGL_KHR_create_context`,
    `EGL_KHR_surfaceless_context`, `EGL_KHR_no_config_context`,
    `EGL_KHR_image_base`, `EGL_EXT_image_dma_buf_import` and
    `EGL_EXT_buffer_age` in its one extension string. No platform-display
    extension string was found, so the Brick's port opens
    `EGL_DEFAULT_DISPLAY` on the null window system, where the Pixel 2's
    opens a GBM display (`diatom/port/pixel2.c:1463`).
  - OpenGL ES 3.2 and GLSL ES 3.20, renderer "PowerVR Rogue GE8300". Its
    extension table names everything the GPU cores and Diatom use
    (`GL_OES_EGL_sync`, `GL_OES_surfaceless_context`,
    `GL_OES_packed_depth_stencil`, `GL_EXT_shader_framebuffer_fetch`,
    `GL_EXT_color_buffer_float`, `GL_OES_EGL_image_external`), but it is the
    driver's whole table (it also names ray tracing, which the GE8300 lacks),
    so what this GPU exposes is read at run time.
  - Diatom's GPU path already takes the KHR sync calls through
    `eglGetProcAddress` and checks wait sync by name, which fits EGL 1.4.
    The driver warns that `glFinish` stalls PowerVR's deferred rendering;
    the Brick's present path uses fences, not the `glFinish` the Pixel 2's
    has at `pixel2.c:1944`.
  These are observations of the shipped files, not of a running Brick: the
  first GPU-core test kit confirms them.
- **Sound**: two front-firing speakers (stereo), a headphone jack at the
  bottom, the ladders already in `brick.c`, moved to `brick_levels.h`.
- **Lights**: RGB lights in a bar on the top edge, in each of the four
  shoulder keys and behind F1 and F2, plus a charge light by the power key
  (TrimUI's store page). They hang off the A133's LED controller (`ledc` in
  the Brick's device tree), which plastron does not drive today. So the
  profile's lights part grows from "side lights" to a list of light groups
  with what each can show; the Brick's first release declares none, and
  driving them (the battery on the top bar, the shoulders lighting with the
  tutorial's keys) is later work.
- **Card**: A/B system slots and System Update from the Brick's first
  release (decided), the same scheme as the Pixel 2: a boot partition with
  `a/`, `b/` and `bootstate.txt`, two root slots, the games partition, and
  a folder outside the slots for the closed files, the player's own copy
  (section 7). The Brick's U-Boot gains the bootstate logic and
  FAT writes it lacks today.
- **Variants**: the Brick has one of six panels (plastron's kernel patch
  0012 picks it at boot), and TortOS's Brick image also serves the "Brick
  Hammer". The profile is one `brick`; what differs between units is found
  at boot, and the first tester kit is the panel probe card plastron
  already has (`board/brick/probe`).

The Brick's front end:

- **Its handheld**, drawn in code like the Pixel 2's: a `td_model` with its
  own colors from the theme's roles and its keys on their faces (the
  shoulders on the back ledge, which a tilt shows, as the Pixel 2's top
  shows its shoulders; F1/F2 and MENU on the front). The outline is
  published: 73.2 mm wide, 109.9 mm tall, 19.9 mm deep at its thick part
  and 11.79 mm at its thin part (TrimUI's store page for the width and
  depths, RetroHandhelds for the height; the store's own "10.99 mm" height
  is a slip for 109.9). The keys' places and sizes are measured from
  TrimUI's six-view product images, scaled to that outline (appendix,
  "Measurements"). Those images are reference only, for proportions,
  layout and key shapes: they are never shipped or traced, and the model
  carries no TrimUI mark. A case maker notes a later color run is slightly
  bigger; the model follows the first run. When I call for testers, one
  owner's ruler checks the numbers. Original art only.
- **The tutorial** keeps its lessons; the Brightness lesson already teaches
  F1/F2, MENU already acts on press. With the model, the Brick's lessons
  shift right and show the handheld as the Pixel 2's do. Lessons the Brick
  has nothing for (the battery on side lights) drop out by capability.
- **The Controls tab** shows the Brick, leaning by its model.
- **Key hints and glyphs**: MENU is drawn as the Brick's MENU key, from the
  profile's printed label and shape; every fit check and shot runs for the
  Brick too, since the glyph's width changes the text around it.
- **Words**: strings that name the Pixel 2's parts (the adapter, the cable,
  the side lights) are already chosen at run time; the About credit "the
  Pixel 2 kernel" says each device's kernel.

The Brick's back end and image:

- `src/device/brick.c` and `diatom/port/brick.c` catch up: the charger-online
  rule, the stubs that its hardware can answer (boot theme, saved panel
  stills, chip temperature, charge reading), Developer Mode's unlock on held
  L2+R2, zyhome's MENU code from the profile.
- plastron's Brick board catches up with the Pixel's shared parts: the
  `zyperos` board pieces (inittab, datapart, the SDL patches), the splash's
  first-start and theme frames, and the fixes the map found (GETFB2,
  `third-party-packages.md`).
- Release artifacts: `ZyperOS-v<ver>-brick.img.xz`, its update lists when it
  has System Update, the installer's Brick entry, the site's Brick button.

### 6b. The TrimUI Brick Pro

The Brick Pro is the Brick's chip in a bigger body (public specs: TrimUI's
store, The Gadgeteer, RetroCatalog, RetroHandhelds):

- **Chip, GPU, memory**: Allwinner A133P, 4 x Cortex-A53 at 1.8 GHz,
  PowerVR GE8300 at up to 660 MHz, 1 GB LPDDR3, 8 GB eMMC, microSD. The same
  as the Brick, so the same kernel family, the same Diatom port, the same
  cores and the same closed GPU files (section 7). An early leak said A523
  and Mali; the shipped device is A133P.
- **Screen**: 3.95 inches, 1024x768, 4:3, IPS, laminated, 60 Hz. The canvas
  is its panel, scale 1, as on the Brick.
- **Body**: 86.6 x 129.9 mm (RetroCatalog), 236 g; about 11.7 mm deep at
  the upper part and 23.6 mm at the lower part, measured from the views
  (no published depth). A 5000 mAh battery.
- **Controls**: the Brick's front keys and back-ledge shoulders, plus two
  Hall-effect sticks (JF15) under the d-pad and the face buttons, with
  L3/R3 clicks and RGB rings, and a HOME key on the left side under the
  volume keys. The right side has the FN slider (a lock switch here) and
  POWER.
- **Ports**: a USB-C host on the back ledge (it also carries DisplayPort
  video out at 1080p60, which ZyperOS does not use), the bottom USB-C for
  charging and a computer, a 3.5 mm jack, stereo speakers, a mono mic.
- **Radios**: Wi-Fi 4 and Bluetooth 4.2; no page names the chip, so its
  firmware is checked in TrimUI's Brick Pro firmware before the image work.
- **Lights**: RGB on the top bar, the four shoulders, F1/F2 and the stick
  rings.
- **Model number**: the views' back label names TG4070 and 5000 mAh.
  TrimUI's store also lists an Android "Brick Hammer Pro U" (Snapdragon
  G2 Gen 1) near that number; that is another device, not a target here.

What it needs beyond the Brick:

- **Its own profile and image** (`brickpro`, `ZyperOS-v<ver>-brickpro.img.xz`),
  built from the Brick's plastron board with its own device tree (panel,
  sticks, HOME), made the way the Brick's was: from the stock device tree
  in TrimUI's own firmware, decompiled. *(My call: one image per device
  keeps the profile a build-time choice; the two share everything else.)*
- **Analog sticks, the first in ZyperOS.** The profile declares them; the
  launcher reads the left stick as the d-pad in menus; Diatom passes both
  sticks to cores as analog (`RETRO_DEVICE_ANALOG`, with a dead zone), so
  the d-pad-as-stick setting becomes a choice, not a need; the Controls
  tab gets stick rows and the handheld model gets its sticks, L3/R3 and
  HOME; the tutorial's Move lesson names the stick too.
- **Its handheld**: a `td_model` from its six-view images, the same way as
  the Brick's (appendix, "Measurements").

### 7. The Brick's closed files

A Brick image needs two sets of closed binaries. I looked for where they
come from, under what terms, and what the other Brick firmwares do.

**Where they come from.**

- **The GPU**: Imagination's PowerVR DDK userspace for the GE8300 (EGL,
  GLES, the null window system that draws to the framebuffer) and the GPU's
  firmware (`rgx.fw.22.102.54.38`). Allwinner ships them in its A133 SDKs to
  the companies that build devices. Two versions are public:
  - **DDK 1.11@5516664**, in Creality's Sonic Pad SDK on GitHub
    (`CrealityTech/sonic_pad_os` at `7f37a7f`,
    `package/libs/libgpu/ge8300/fbdev/glibc`). It matches the GPU kernel
    driver in plastron's Brick kernel, which comes from the same SDK tree
    (Imagination's kernel side is dual MIT/GPL, so that part is open).
    plastron's `pvr-ge8300` package checks exactly these files
    (`files.sha256`).
  - **DDK 1.19**, in TrimUI's own firmware for the Brick, which TrimUI
    publishes as zip files on GitHub (`trimui/firmware_brick`, newest
    v1.1.1-20251126). Its kernel module is 1.19 too, so these libraries do
    not load on plastron's 1.11 kernel driver.
- **The Wi-Fi and Bluetooth**: XRadio's firmware for the XR829
  (`boot_xr829.bin`, `fw_xr829.bin`, `sdd_xr829.bin`, `fw_xr829_bt.bin`, the
  24 MHz ones), in the same Sonic Pad SDK
  (`package/firmware/linux-firmware/xr829`) and in TrimUI's firmware.

**Under what terms.** Neither set comes with a license file or any
redistribution terms, in the Sonic Pad SDK, in TrimUI's releases or in the
other projects below. Creality's repository is marked GPL-2.0, but that is
its build system; it does not license Imagination's or XRadio's binaries.
Imagination supports only the companies that license its driver kit. So
there is no written permission to pass the files on, and none found that
forbids it either.

**What the other Brick firmwares do.**

| Project | Which files | How they get to the player |
|---|---|---|
| TortOS (the same plastron Brick board as ZyperOS's) | DDK 1.11 and the XR829 firmware, from the Sonic Pad SDK | inside its public Brick image since 2.0.0 (2026-10-26); a `PROPRIETARY.txt` names them; left out of the sources tar |
| Knulli | DDK 1.19 from TrimUI's stock libraries, over the Sonic Pad files; TrimUI's kernel and modules as binaries | its own public GitHub repository (`knulli-cfw/ge8300-drivers`), which its build downloads, and its public images |
| Slot OS (a Brick frontend on its own card) | TrimUI firmware 1.1.1's kernel, modules, DDK 1.19 and XR829 firmware | inside its public image; its NOTICE names them as TrimUI's, "passed on unchanged" |
| NextUI, MinUI | none | copied onto a card that the Brick's own system starts, so they use TrimUI's libraries in place |

So every image-based Brick firmware I found ships the files in public, with
a notice. The risk is a takedown request from Imagination, XRadio, Allwinner,
Creality or TrimUI against the release files, not a license term that is
broken knowingly; nobody has written permission.

**The ways ZyperOS could do it.**

1. **In the image, as TortOS does.** plastron already builds it this way:
   the files go into the root, and `PROPRIETARY.txt` names them. The least
   work. A takedown would hit the release's images and System Update packs.
2. **The player's own copy.** ZyperOS's images and System Update packs
   carry none of them. The installer, on the player's computer, downloads
   the files from where their publisher put them (the Sonic Pad SDK at its
   pinned commit), checks each against `files.sha256`, and writes them to a
   folder on the card outside the system slots; the system loads them from
   there at boot, and System Update leaves that folder alone. The same
   pattern as Broadcom's `b43-fwcutter` or Asahi Linux taking Apple's
   firmware from the owner's own macOS. Costs an installer step, that folder
   in the Brick's card layout (decided before the layout is fixed, step 6),
   and a dependency on Creality's repository staying up.
3. **The player's own copy, from TrimUI.** As 2, but from TrimUI's firmware
   zip, the device maker's own release. The GPU kernel driver must move to
   DDK 1.19 first, and its source must be found (it is MIT/GPL, so a newer
   Allwinner kernel tree may carry it). The most work; the source least
   likely to disappear.

4. **No closed files at all.** The launcher draws with the CPU (SDL's
   software renderer straight to the framebuffer), Diatom uses the CPU path
   it already has on the Brick, the GPU-frame systems stay hidden, and Wi-Fi
   comes only from a USB Wi-Fi adapter on the back port (the Pixel 2's
   adapter, whose firmware is in linux-firmware and may be passed on), which
   needs USB host turned back on in the Brick's kernel. No Bluetooth. Every
   byte of the image is open. The costs: the launcher's speed on the CPU is
   not measured yet, there is no built-in Wi-Fi or Bluetooth, and the
   adapter takes the controller port.

**Can ZyperOS make its own versions instead?** I looked; not in a useful
way, and not faster than the vendor's:

- **The GPU libraries** are a whole GPU driver (the GLSL compiler, the
  shader core's instruction set, the firmware interface), with no public
  documentation for this version. The open route exists only in part:
  Mesa's open PowerVR driver lists the GE8300 since Mesa 25.3 but marks it
  unsupported and not under active development, it is Vulkan only (GLES
  would go through Zink) and not conformant on it; it needs the mainline
  kernel's `drm/imagination` driver, which names other Rogue cores, not the
  GE8300; and the A133's mainline display support is partial (no DSI panel
  yet). Not for 1.0.6, and today it would be slower and less complete than
  the vendor's driver. Worth watching: if it matures, the Brick gets an
  open GPU stack on a mainline kernel.
- **The GPU firmware** (`rgx.fw`) runs inside the GPU and must match the
  driver; it is not ours to write.
- **The XR829 firmware** runs on the Wi-Fi chip's own processor; there is
  no documentation and no open firmware for it.

What ZyperOS can make faster is how it uses the GPU (fewer draws, kept
textures, as on the Pixel 2), not the vendor's driver itself.

**Reverse engineering them** (static analysis of the aarch64 libraries and
the firmware blobs, with REA and Ghidra) does not change this. The useful
GPU set is about 5 MB of code: a whole OpenGL ES 3.2 stack, a GLSL compiler
and a shader-core back end. Rewriting it from what we learn is an open GPU
driver project on the scale of Panfrost or Asahi's GPU driver, years for a
team, and it would rediscover what Imagination's own open Mesa driver
already documents (the Rogue instruction set, its compiler, its firmware
interface); the register-level setup is already open in the MIT/GPL kernel
driver. The XR829 firmware is a Wi-Fi MAC for an undocumented chip. Where
reverse engineering pays is small and bounded: reading which EGL and GLES
features DDK 1.11 offers (fence sync, surfaceless contexts, wait sync, the
GLES version), the main unknown for Diatom's GPU frames on the Brick
(section 6), before step 6, with no device. That check is done (section 6,
"GPU"): the API Diatom needs is there. Any reverse engineering keeps
the clean-room rule: notes on behavior only, and nothing of the binaries
in the repository.

**Measured: drawing the launcher with the CPU is not good enough.** On
2026-11-14 a test build of the launcher (scratch branch `swdraw-spike`,
never merged) ran on my 32 GB Pixel 2 test card in two modes: as shipped
(GLES2), and with SDL's software renderer drawing every frame into a
1024x768 picture that the GPU only copied to the panel. Frames that reached
the panel, counted by `px2-shot --fps` over 4 s with the same key presses:

| Scene | GPU, as shipped | CPU drawing |
|---|---|---|
| Home shelf, moving left and right | 48.0 fps | 24.2 fps |
| MENU, stepping down and up its list | 34.8 fps | 3.8 fps |
| Tutorial, the first lesson (3D handheld) | 42.5 fps | 7.8 fps |

The Pixel 2's CPU (4 x A35 at 1.3 GHz) is slower than the Brick's (4 x A53
at 1.8 GHz), so the Brick would do better, but not by the 8 to 9 times MENU
and the tutorial need: the blurred backdrops, the panels' alpha and the 3D
handheld are GPU work. Option 4 would mean redrawing the launcher for a
CPU, against performance first. **So option 4 is out**, and the GPU files
are needed for the launcher itself, not only for the GPU games.

**Decided (2026-11-14): 2, the player's own copy**, with 3 as its
fallback if the Sonic Pad files ever go away. How it works:

- ZyperOS's Brick and Brick Pro images, System Update packs and test kits
  carry none of the closed files.
- The installer, on the player's computer, downloads them from the Sonic
  Pad SDK at its pinned commit, checks each against plastron's
  `files.sha256`, and writes them to the card's folder for them, outside the
  system slots. It does this on a fresh install and on an installer Update
  whose card lacks them; it never sends them anywhere else.
- At boot the system takes the GPU libraries, the GPU firmware and the
  XR829 firmware from that folder (the builder picks how: links, a bind, or
  a library path). System Update never touches the folder.
- A card without the files says so on its first screen, in words a player
  can act on ("Run the ZyperOS Installer's Update on a computer"), rather
  than a black screen.
- If the download fails, the installer says why and installs nothing; a
  later version may take the files from a folder the player picks.

This was my recommendation: It keeps ZyperOS's own
release files clean of anything without a license, and it costs one
installer step that testers exercise from the first test build. **This is a
legal decision for me, not a technical one**: 1 is what TortOS and Knulli
do and is the least work; 2 and 3 avoid passing the files on. A lawyer's
look is worth it before the first public Brick release, whichever way.

### 8. Testing on Bricks I do not have

The Brick work is tried by Brick owners on the ZyperOS Discord.

- **On the Mac first.** Every Brick screen is a host shot with
  `--device brick`; each build's contact sheet shows what changed, so a
  tester's time goes to what only the device can show.
- **A kit per build**, posted as a release-like page outside the public
  releases (a draft release or a test folder): the image for a fresh
  install (later, a payload or a System Update pack for a quick update),
  its SHA256SUMS, what changed, and a short numbered test list of 10 to 15
  checks, each with what to look for and what to send.
- **The first kit is the panel probe** (`plastron/buildroot/board/brick/probe`,
  already in the tree): a card that only shows and saves which of the six
  panels is in the Brick. It needs none of the closed files, and it tells us
  the testers' units before anything else.
- **Results come back as logs.** The tester opens File Transfer's page over
  the Brick's Wi-Fi and presses Download logs; the log pack's `about.txt`
  gains the device id, the build, and a short hardware report (panel, input
  devices, GPU driver version, Wi-Fi chip up or not). Where Wi-Fi itself is
  what fails, the tester takes the card out and sends the same files from
  `.userdata/brick/logs` with a computer. Plus a photo where the screen is
  the result. Testers post them in one Discord thread per kit; I bring them
  back.
- **The call for testers** goes out when there is something to test, not
  before: I post it on Discord myself. It asks for each tester's exact model
  (TG3040 Brick, Brick Hammer or Brick Pro), its color and a spare card,
  and for one ruler check of the handheld model's numbers.
- **The pass**: a kit's test list done by at least one tester with no
  failure left open is the `Tested on` line for that version (section 5).

### 9. Order of work

Each step is one PR to `main`, after `main-split-2` and `design-system-2`
are merged. Steps 1 to 4 make ZyperOS ready for other devices and change
nothing on the Pixel 2; they are 1.0.6. Steps 5 to 7 are the Brick: step 5 is
host-only and can be in 1.0.6; steps 6 and 7 need testers and ship in the
first release after the Brick passes its test list (1.0.6 if that work is
done by then).

1. **The device profile.** `device_profile`, the `pixel2`, `brick` and
   `desktop` profiles, `plat_profile()`, the host `--device` flag,
   capabilities in place of `fn_volume` and of the side-effect guesses,
   `plat_system_has` (today's file checks behind it; the image's `has` line
   comes in step 4), the generated `<id>.mk`/`<id>.h`,
   `make check-devices`, the Brick files compiled in CI. Fixes on the way:
   Developer Mode's unlock, the Controls tab's handheld shown only with a
   model. Pixel gate clean.
2. **The handheld as data.** `td_model`, the Pixel 2's model in
   `pixel2.model.c`, turn and lean from the model, kept boxes per model.
   Pixel gate clean.
3. **Systems, cores and power by capability.** `needs|` lines, the payload's
   cores per device, core option defaults per device, the power steps in the
   Diatom port, `vendor/<arch>/`, `-mcpu` from the profile.
4. **Build, card and release by device.** plastron's device symbol, one
   card script with `device.sh`, System Update's device and slots and the
   `has` line in `/etc/zyperos/system`, `tools/release.sh` over a device
   list, the installer's devices from the release, dev tools with a device.
   A Pixel 2 release built through the new path is byte-for-byte the same
   payload as one built through the old. This step reads the card layout
   from the image for every device; the Brick's own numbers are written in
   step 6.
5. **The Brick's front end.** Its profile, a first handheld from the
   published outline and then from the testers' measurements, its MENU
   glyph and key hints, its tutorial and Controls tab, all on the Mac; a
   contact sheet of every Brick screen for me to look at. The README and
   i18n shots move to `--device pixel2` here, on their own sheet. The panel
   probe kit goes out to testers during this step.
6. **The Brick's back end and image.** Drivers, Diatom port, card script,
   plastron's Brick board with A/B slots, the bootstate logic in its U-Boot,
   the closed files as the player's own copy (the installer's step and the
   boot's folder, section 7), the log pack's hardware report; test kits for
   the testers until a kit passes (testers install through the installer
   from the first kit that needs the GPU).
7. **The Brick's release.** Release artifacts, installer with the
   player's-own-copy step, site, docs; only after a kit passes and I say
   release.
8. **The Brick Pro.** Its profile and handheld (host, can come with step
   5), its device tree from TrimUI's firmware, analog sticks in the
   launcher, Diatom, the Controls tab and the tutorial, its image; test kits
   for Brick Pro owners.
9. **The Brick Pro's release.** As step 7, for `brickpro`.

The CPU-drawing measurement (section 7) ruled out a Brick with no GPU
files.

## What is left to the builder

- The C shape of `device_profile` and `td_model`, how the profile reaches
  the generated `.mk`/`.h` files, and the names of the new files, within the
  rules above.
- How the shot checks loop over devices, and which existing shot flags are
  kept as aliases.
- The order of changes inside each PR, and how each PR proves the Pixel 2 is
  unchanged (the pixel gate, payload diffs, a device run on my test card
  where a PR changes the image).
- Where on the Brick's card the closed files' folder sits, and how the
  system finds them at boot.

## Calls made in this design

Each is marked *(My call)* where it is written:

- The host's default profile copies today's answers, so refactor PRs keep a
  clean pixel gate; real-device shots use `--device`.
- What the hardware has (profile) and what the system has
  (`plat_system_has`, from `/etc/zyperos/system`) are two facts; no
  `min-system` raise.
- The device files' Linux halves compile in a Linux CI job and, where the
  SDK is, in `make check-devices`.
- Core option defaults per device, read at run time; the player's own
  choices stay shared; a one-time cleanup of seeded Pixel 2 defaults.
- One core set per architecture at the `cortex-a53` baseline.
- Every release carries every released device and waits for each one's
  `Tested on` line.

## Decided

- **The design**: approved as written, with the calls above (2026-11-14).
- **Hardware**: I have no Brick; Brick owners on Discord test (section 8).
- **System Update on the Brick**: A/B slots and System Update from its first
  release.
- **The Brick Pro**: added as the third device, from TrimUI's reference
  views (section 6b); the Brick's measurements may come from online
  sources.
- **The closed files**: the player's own copy, fetched by the installer
  (section 7). A Brick with none of them is out (measured).

## Decisions left

None for the plan. Later questions belong to the steps that meet them
(the lights' uses, the Brick Pro's sticks in each system).

## Appendix: measurements

Taken from TrimUI's six-view product images (the 1600 px views of the
purple Brick and of the Brick Pro), by pixel, scaled to the published
outlines: the Brick at 7.0 px/mm (73.2 x 109.9 mm), the Brick Pro at
5.55 px/mm (86.6 x 129.9 mm). Reference only: the images are not shipped,
not traced, and nothing of TrimUI's marks goes on the models. Millimetres
from the front's top-left corner, x to the right, y down; key places are
centers; good to about half a millimetre.

| | Brick (TG3040) | Brick Pro |
|---|---|---|
| Outline | 73.2 x 109.9 (published) | 86.6 x 129.9 (published) |
| Corners, front | top r 0.5, bottom r 4.1 | top r under 1, bottom r 5.4 |
| Depth | 11.8 above the ledge, 19.9 below (published 11.79 / 19.9) | 11.7 above, 23.6 below (measured) |
| Back ledge (top of the thick part) | y 42.9 | y 49.1 |
| Screen glass | x 0.9 to 72.2, y 0.8 to 57.2 | x 1.3 to 85.1, y 0.4 to 66.2 |
| Active screen (from its size) | 65.0 x 48.8, 3.2 in, centered in the glass | 80.3 x 60.2, 3.95 in |
| D-pad | center (15.9, 72.3), span 19.3, arm 7.0 wide | center (17.4, 81.2), span 20.0 |
| F1, F2 (pills) | (30.9, 63.6), (41.6, 63.6); 7.1 x 2.6 | (36.8, 73.2), (49.4, 73.2); about 8 wide |
| Face diamond | center (56.3, 72.1), keys 7.6 from it, each 7.7 across | center (69.0, 81.2), keys 7.5 from it |
| Sticks | none | (25.2, 104.9), (61.2, 104.9); caps 13.5 across, rings about 15 |
| Bottom round keys | (26.1, 101.4), (36.1, 101.4), (46.2, 101.4); 5.4 across | (33.2, 121.7), (43.1, 121.7), (53.2, 121.7) |
| Speaker grilles | (11.1, 99.6), (61.9, 99.6); 7.4 wide, two dot rows | (14.5, 121.7), (71.7, 121.7) |
| Left side | VOL+ at y 10.3, VOL- at y 21.4, each 7.7 long | VOL+ y 17.9, VOL- y 30.9, HOME y 51.8 (8.5 long) |
| Right side | FN slider at y 23.3 (10.3 long), POWER at y 39.3 (7.1 across) | FN slider y 22.5 (9.7 long), POWER y 39.7 |
| Shoulders (on the back ledge) | L1 and R1 outside, about 17.5 wide; L2 and R2 inside, about 11; the USB-C housing between, about 13.5; keys from y 41.3 to 47.0 | the same order; keys from y 50.1 to 59.5 |
| Top edge | a light bar 37.9 wide, centered, at the back | a light bar, centered |
| Bottom edge, left to right | microSD, a small hole, USB-C (center), a pinhole, 3.5 mm jack | the same |

## Appendix: the map

Status words: **present** (there and plausible), **stub** (answers false,
0 or nothing), **rotted** (was right, is now wrong or would not build),
**missing**. Lines are at `aded88d`.

### Launcher, platform layer

| Where | What | Brick |
|---|---|---|
| `src/platform.h:20-21` | the 1024x768 canvas, the Brick's panel | present |
| `src/platform.h:47` | `PLAT_TARGET_FORMAT` ABGR8888, for GLES2's byte order | present |
| `src/platform.h:333-334` | `PLAT_VOL_MAX 20`, `PLAT_BRIGHT_MAX 11`: a contract with Diatom and launch.sh | present |
| `mk/cross.mk:6`, `mk/native.mk:10` | `DEVICE ?= brick` | — |
| `mk/cross.mk:11` | `-mcpu=cortex-a53` for every device and every helper | present |
| `src/device/brick.c:1007-1059` | the host stubs: the Mac build is the Brick's file | rotted (two jobs) |
| `src/device/brick.c:1111` | `plat_gpu_shaders` true when headless, to stand in for the Pixel 2 | rotted |
| `src/device/brick.c` | stubs: boot theme (71), saved stills (147-149), USB group (820-831), brightness raw (1072), side lights (1128-1130), RAM idle (1133), chip temp and charge read and limit (fake files only, 1141-1187), battery lights, Player 2, menu held, tap held | stub |
| `src/device/brick.c:1218-1233` | battery without the charger-online rule (`pixel2.c:1782`) | rotted |
| `src/device/brick.c:155` | SDL render hint "opengl"; launch.sh's env (pvrfb, opengles2) wins | rotted (comment) |
| `src/device/pixel2.c` | shared-in-all-but-name: splash signal, input scan/poll/wait, jack, ALSA control structs, level save/nudge; KMS scale and turn with 640/480/320/240 literals (215-241, 306, 334); USB gadget `px2` (1095), usb0 10.42.0.1/30 (1107-1145), role switch `ff300000.usb-role-switch` (1031), DMC cap (1598), side-lights file under `.userdata/pixel2` (1562), thermal zone names (1632) | — |
| `src/device/pixel2_levels.h` | ladders shared with portkeys and Diatom; the Brick's are inline in `brick.c:571-597` | missing (header) |

### Launcher, screens and features

| Where | What | Brick |
|---|---|---|
| `src/tour.c`, `tourdraw.c`, `controls.c` | `fn_volume` = brightness chord + MENU on release + handheld + Home row | present (Brick branch) |
| `src/tourdevice.c:31-143`, `tdkey.h` | the Pixel 2's body, keys, poses and sides, hand-copied from `tools/build-pixel2-model.py` | missing (by design) |
| `src/tourdevice.c:145-149` | handheld shown only with `fn_volume` | — |
| `src/ctltab_screen.c:30-49`, `main.c:20226` | Controls tab handheld, sized for the Pixel 2, shown on every device | rotted (a Brick shows a Pixel 2) |
| `src/ctltab.c:298-317` | lean from the Pixel 2's sides | — |
| `src/ui.c:462-506`, `2226-2232`, `2513-2554` | every "MENU" drawn as the Pixel 2's three-dot key | rotted |
| `src/controls.c:39`, `101-102`, `198` | "MENU+VOL" or "F1/F2"; L2+R2 battery row; MENU+START Home (zyhome) | present |
| `src/main.c:13421` | Developer Mode unlock via `plat_tap_held` | rotted (never unlocks) |
| `src/main.c:4307-4316` | shared port and built-in Wi-Fi guessed from `plat_usb_peer` | — |
| `src/main.c:8383-8930` | Flash Drive pages saved at 640x480, 5/8 scale | missing |
| `src/portload.c:52-61`, `tools/genfirststart.py:61`, `res/loading`, `res/boot/first-start` | panel-pixel art for the Pixel 2 | missing |
| `src/main.c:9666`, `5816`; `res/web/app.js:2150` | device id from `P_USERDATA`'s basename; `pixel2` fallback | — |
| `src/hotspot.c:43-64` | hotspot found by sysfs and hostapd | — |
| `src/p2.h:70` | `P2_CABLE_ADDR` "10.42.0.1", the Pixel's | rotted |
| `src/main.c:10141-15334` | mDNS interfaces "usb0", "wlan0" as literals | — |
| `src/heat.h:29`, `charging.h:83-84`, `status.h:68`, `battery_eta.h:96` | the RK3326's trip, RK817 charge curve, current sign | — |
| `src/db.c:46-215` | core option defaults tuned on Panfrost, in the shared `library.db` | — |
| `res/strings/about.txt:238` | "the Pixel 2 kernel" on every device | rotted |
| DESIGN.md §1, §2, §4, §11.9, 11.10, 11.12, 11.18-11.20, 11.25-11.28, 11.35, 11.36, 11.43, 11.45 | device specifics stated as rules | — |
| `tools/tour-shot-check.py:155-176`, `246-251`; `controls-shots-check.sh`; `i18n-shots.py`; `readme-shots.py` | Pixel 2 pixel boxes; "the Brick has no handheld" | — |
| `simulator/` | the Pixel 2's frame, keys, lights; the VM is the Pixel 2's | — |

### Card scripts and helpers

| Where | What | Brick |
|---|---|---|
| `sd/brick/launch.sh` | unchanged since 1.0.0: no ports, `run:`, PortMaster, portkeys, loading still, zyhome, reboot/restart flags, usbdrive, PPSSPP assets, VMU move | rotted |
| `mk/payload.sh:24-28`, `39`, `223`, `230-246` | launch.sh by device; p2d, PortMaster, OpenBOR for the Pixel 2; `bt-alsa.sh` for the Brick | — |
| `tools/portkeys.c`, `pkkeys.h:37-39` | the Pixel's MENU code, DRM cursor plane, ladders | missing |
| `tools/zyhome.c:38` | `CODE_MENU BTN_TRIGGER_HAPPY1` | rotted (Brick's MENU is `BTN_MODE`) |
| `res/portmaster/control.txt:53-81` | `gkd-pixel2`, RK3326, 640x480, no sticks | missing |

### Diatom and cores

| Where | What | Brick |
|---|---|---|
| `diatom/Makefile:13`, `94-121` | `PORT ?= desktop`; per-port sources and libraries | — |
| `diatom/include/diatom_port.h` | 37 functions + caps | — |
| `diatom/port/brick.c` | all 37 defined; never compiled by `make check` | rotted risk |
| `diatom/port/brick.c:2120-2127`, `1209`, `2116`, `1602`, `1932` | GPU frames, power, strength, rumble stubs | stub |
| `diatom/port/brick.c:1592-1625` | only the largest plane (DS shows one screen) | stub |
| `diatom/port/pixel2.c:1463-1505`, `1866-2140` | KMS+GBM+EGL, GLES 3, EGL_KHR_fence_sync required | — |
| `diatom/port/pixel2.c:2618-2673` | fast clock and the light/medium table (CPU, DMC, GPU devfreq paths) | stub |
| `config/systems.cfg:189`, `229`, `251`, `314`, `360-361` | GPU-frame systems, with no flag | missing (shelves fail) |
| `config/systems.cfg:287`, `367-369` | OpenBOR and Ports, Pixel 2 only by comment | — |
| `mk/fetch-vendor.sh:36-37`, `mk/core-stamp.sh`, `mk/payload.sh:145` | one flat aarch64 core set | — |
| `mk/build-ppsspp.sh:105` | `-mcpu=cortex-a35` | — |

### plastron, release, installer, site

| Where | What | Brick |
|---|---|---|
| `package/zyperos/zyperos.mk:45`, `diatom.mk:11`, `splash.mk:14`, `external.mk:14-34` | device guessed from PowerVR or the DTS name | — |
| `package/zyperos/zyperos.mk:67-76`, `123-133` | p2d, zyhome, portkeys, zyperos-update and `/etc/zyperos/system` for the Pixel 2 only | missing |
| `package/zyperos/zyperos.mk:140-162` | SSH tools built for every device; `px2-shot.c:68` GETFB2 against 4.9 headers | rotted (expected, not built) |
| `board/zyperos` | shared parts used by px2 and QEMU, not the Brick | missing |
| `board/brick/genimage.cfg` | GPT, `BRICKBOOT` at 4M, one 256M root, state, games at p4 | — |
| `board/brick/third-party-packages.md` | no xz; release stops at the compare | rotted |
| `update/slots.c:85-87`, `update/main.c:1420`, `update/apply.sh:36` | p1/p2/p4 and `pixel2` default | rotted for the Brick |
| `tools/release.sh:53-64`, `181-185`, `221`; `.github/workflows/release.yml:250` | Pixel 2 only | missing |
| `installer/` `cards.rs:58-63`, `release.rs:71-76`, `install.rs:45-70` | both labels and names; Brick offered, download fails; update writes p2 | rotted |
| `site/index.html`, `download/`, `downloads.js:22`, `llms.txt`, blog | Pixel 2 only; "Brick image comes later" | missing |
| `docs/guide.md`, `building.md:389-395` | half TortOS-era Brick text | rotted |
| `tools/px2`, `px2-deploy.sh`, `px2-shot.c`, `px2-lat.c`, `px2-perf.c` | Pixel 2 address, key, DRM | missing |

### What a Brick build needs from outside the repository

- `BRICK_GPU`: the PowerVR DDK 1.11@5516664 userspace for the GE8300
  (libEGL, libGLESv2, libGLES_CM, libIMGegl, libsrv_um, libglslcompiler,
  libusc, libpvrNULL_WSEGL, `rgx.fw.22.102.54.38`, Khronos headers), checked
  against `package/pvr-ge8300/files.sha256`.
- `BRICK_XR829`: `boot_xr829.bin`, `fw_xr829.bin`, `sdd_xr829.bin`,
  `fw_xr829_bt.bin`, checked against `package/xr829-firmware/files.sha256`.
- The Allwinner 4.9.191 kernel base tarball, sha256-pinned, downloaded by
  `scripts/build-brick-kernel.sh`; the `brick-kernel` and `brick-boot`
  Docker images. U-Boot and TF-A tarballs only to rebuild the committed boot
  loader.
