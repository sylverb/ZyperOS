# ZyperOS on more than one handheld: a device profile, then the TrimUI Brick

## Intent

ZyperOS runs on one handheld today, the GKD Pixel 2. For 1.0.6 I want it
ready for other handhelds, possibly with other CPUs, and then I want the
TrimUI Brick to be the second one. ZyperOS grew out of TortOS, which ran on
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
- **Room for another CPU.** The core set, the build flags and the release
  carry an architecture, even while every device is aarch64.

## Non-goals

- **One image for every device.** Each handheld keeps its own image, built
  from its own plastron board. The kernels, boot loaders and GPU drivers
  differ anyway.
- **A screen that is not 4:3.** The launcher keeps drawing 1024x768. A 16:9
  device is a later design; the profile carries the aspect so such a device
  can say it does not fit yet.
- **Analog sticks, touch screens, L3/R3.** Neither device has them.
- **Building cores for armhf or x86_64 now.** The architecture becomes a
  name in the paths and the release; only aarch64 is filled in.
- **A third device in 1.0.6.** The TrimUI Brick Pro (Allwinner A523, Mali)
  and the RK3566 handhelds are what the profile should be able to take
  later; none is added here.
- **Renaming the `px2-*` debug tools.** They gain a device; their names stay.

## Anti-goals

- **A Pixel 2 that gets worse.** No dropped frame, no slower start, no
  changed pixel, no lost feature, because of the profile.
- **A device switch in the UI.** No `if (brick)` in a screen. A screen reads
  a capability or a piece of the profile's art, and draws.
- **A Brick that offers what it cannot do.** No GPU shelf that fails to
  load, no Player 2 that never connects, no lesson for side lights it does
  not have, no System Update row that cannot update.
- **Somebody else's art.** The Brick's handheld is drawn in code from my
  own measurements of my own Brick, as the Pixel 2's was, never from other
  people's renders, packs or photos.
- **Closed files in git.** The Brick's GPU libraries and Wi-Fi firmware stay
  outside the repository, as today.

## Constraints

- **No Brick at this desk today.** Everything up to the Brick's image is
  built and checked on the Mac; the image, the drivers and the GPU cores need
  the device.
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
| **Screen** | panel width and height in pixels, its turn (0, 90, 270), the canvas's scale to it (0.625 on the Pixel 2, 1 on the Brick), the aspect (4:3 only for now), whether a still of the panel can be saved for a port's loading screen |
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
- **Buttons**: d-pad, A B X Y, L1 R1 L2 R2 (L2 and R2 plain buttons), SELECT,
  START, MENU (a real button, `BTN_MODE`), F1 and F2 on the front
  (brightness), VOL+ and VOL-, POWER, a mute switch.
- **Radios**: Wi-Fi and Bluetooth built in (XR829). A hotspot for Player 2:
  off until it is tried on the device.
- **USB**: a gadget link to a computer (10.55.0.2), no host role, no power
  out, no phone tethering, no Flash Drive at first.
- **Battery**: AXP2202, with the charger-online rule the Pixel 2 needed; no
  charge limit until the driver has one.
- **GPU**: PowerVR GE8300 through Imagination's closed GLES; GPU frames
  **off** until a GPU core runs on it (Diatom's fbdev EGL path is new work);
  the old-screen shaders stay the CPU's Sharp map, as today.
- **Sound**: two speakers (stereo), the ladders already in `brick.c`, moved
  to `brick_levels.h`.
- **Lights**: whatever the Brick has, once I look at mine; none until then.
- **Card**: one card layout for every device, with A/B system slots, if the
  Brick gets System Update in its first release (open question below).

The Brick's front end:

- **Its handheld**, drawn in code like the Pixel 2's: a `td_model` from my
  measurements and photos of my own Brick, its own colors from the theme's
  roles, its keys on their faces (shoulders on top, F1/F2 and MENU on the
  front). Original art only.
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

### 7. Order of work

Each step is one PR to `main`, after `main-split-2` and `design-system-2`
are merged. Steps 1 to 4 make ZyperOS ready for other devices and change
nothing on the Pixel 2; they are 1.0.6. Steps 5 to 7 are the Brick: step 5 is
host-only and can be in 1.0.6; steps 6 and 7 need the device and ship in the
first release after the Brick passes its device test (1.0.6 if the device
work is done by then).

1. **The device profile.** `device_profile`, the `pixel2`, `brick` and
   `desktop` profiles, `plat_profile()`, the host `--device` flag,
   capabilities in place of `fn_volume` and of the side-effect guesses,
   `plat_system_has` (today's file checks behind it; the image's `has` line
   comes in step 4), the
   generated `<id>.mk`/`<id>.h`, `make check-devices`, the Brick files
   compiled in `make check`. Fixes on the way: Developer Mode's unlock, the
   Controls tab's handheld shown only with a model. Pixel gate clean.
2. **The handheld as data.** `td_model`, the Pixel 2's model in
   `pixel2.model.c`, turn and lean from the model, kept boxes per model.
   Pixel gate clean.
3. **Systems, cores and power by capability.** `needs|` lines, the payload's
   cores per device, core option defaults per device, the power steps in the
   Diatom port, `vendor/<arch>/`, `-mcpu` from the profile.
4. **Build, card and release by device.** plastron's device symbol, one
   card script with `device.sh`, System Update's device and slots and the
   `has` line in `/etc/zyperos/system`, `tools/release.sh` over a device list, the
   installer's devices from the release, dev tools with a device. A Pixel 2
   release built through the new path is byte-for-byte the same payload as
   one built through the old. This step reads the card layout from the image
   for every device; the Brick's own numbers are written in step 6, after
   open question 3, so step 4 does not wait for it.
5. **The Brick's front end.** Its profile, its handheld (after I measure
   it), its MENU glyph and key hints, its tutorial and Controls tab, all on
   the Mac; a contact sheet of every Brick screen for me to look at. The
   README and i18n shots move to `--device pixel2` here, on their own sheet.
6. **The Brick's back end and image.** Drivers, Diatom port, card script,
   plastron's Brick board; built with the closed inputs and tried on the
   device with a spare card.
7. **The Brick's release.** Release artifacts, installer, site, docs; only
   after its device test passes and I say release.

## What is left to the builder

- The C shape of `device_profile` and `td_model`, how the profile reaches
  the generated `.mk`/`.h` files, and the names of the new files, within the
  rules above.
- How the shot checks loop over devices, and which existing shot flags are
  kept as aliases.
- The order of changes inside each PR, and how each PR proves the Pixel 2 is
  unchanged (the pixel gate, payload diffs, a device run on my test card
  where a PR changes the image).

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

## Open questions for me

1. **Hardware.** Which Brick do I have (the TG3040, not the Brick Pro), and
   a spare card for it? It decides when steps 5 to 7 can start: the art
   needs measurements, every Brick step after 5 needs the device.
2. **The closed files.** Where do the PowerVR libraries and the XR829
   firmware come from, and may a public Brick image carry them? Without them
   there is no Brick image; without the right to ship them there is no public
   one.
3. **System Update on the Brick from its first release.** A/B slots on the
   Brick (the same card layout as the Pixel 2, more plastron and U-Boot work
   first) or one root and installer-only updates at first? The layout is
   fixed at install, so changing it later means every Brick card is
   installed again.

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
