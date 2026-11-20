# ZyperOS Simulator

A GKD Pixel 2 on the Mac's screen, as Apple's Simulator shows an iPhone: the
device drawn to its measurements, its screen showing ZyperOS running in QEMU
([qemu.md](qemu.md)), its buttons pressed with the mouse or the keyboard, and
the parts a player can't press - the battery, the charging cable, the
headphones - set from a panel beside it. What the device shows of them shows
too: the side lights, the backlight, the motor.

For the launcher's menus alone, desk mode is quicker: `make desk` runs the
launcher itself in a window as the Pixel 2, at 60 frames a second, a game and
its in-game menu included ([building.md](building.md), "Work on the UI without
a device"). The simulator is for the whole system: the boot, the setup, the
battery and the charger, the side lights.

## Building and opening it

```sh
brew install qemu
tools/qemu-run.sh fetch
simulator/scripts/build-app.sh
open "simulator/build/ZyperOS Simulator.app"
```

It needs QEMU and a QEMU image, as `tools/qemu-run.sh` does: `fetch`
downloads the published image, checked against `tools/qemu/image.pin`, or
plastron's `make qemu` builds one ([qemu.md](qemu.md), "Setting it up";
while the pin names no image, `fetch` says so). The window starts its VM
with it, by the name `simulator` (`--args --name NAME` for another, `--image DIR` for an image
somewhere else). A VM already running by that name is shown as it is, and
stays running when the window closes; one the window started shuts down with
it. The VM's sound plays on the Mac. It boots the fast way, its kernel on
Apple's hypervisor; the card's U-Boot and its system slots, for System
Update, are `tools/qemu-run.sh start --uboot`, several times slower
([qemu.md](qemu.md), "How it boots").

## Using it

- **Buttons.** Click and hold any of them: the d-pad (it rolls between
  directions as the pointer moves), A B X Y, SELECT and START, the shoulder
  keys along the top, the volume rocker on the left side, MENU and POWER on
  the right. The keyboard: arrows, x z s a (A B X Y), q w e r (L1 R1 L2 R2),
  Return (START), Right Shift or Delete (SELECT), Esc or m (MENU), = and -
  (volume), p (power), h (headphones). A key held is a button held. It is
  the one key map of `src/device/keymap.h`, desk mode's too, by the keys'
  places on a US keyboard. The pointer over a button shows its key on a
  small keycap beside it, and holding Option shows every one.
- **Power.** POWER is ZyperOS's power button. When the VM is off (after Power
  Off, or Auto Off), POWER turns it on again.
- **The panel** (View > Hardware Panel, ⌘I, or the sidebar button under the
  device): the battery level, the charging cable, the headphones; what the
  backlight, the side lights and the motor are doing; the keyboard's keys;
  Power, Shut Down, Save Screenshot.
- **The line under the device**: whether the VM is on, and how many frames a
  second reach the window and which way (`shared memory`, `D-Bus` or `VNC`,
  below); off, what `qemu-run.sh` last said.
- **The Device menu:** Power Button (⇧⌘P), Shut Down, Charging Cable (⇧⌘U),
  Battery levels, Headphones (⇧⌘H), Volume Up and Down (⌘↑, ⌘↓), Save
  Screenshot (⌘S, to the Desktop), Copy Screen (⌃⌘C).
- **Zoom** (View, ⌘1 to ⌘4): at 100% the screen's 640 pixels are the Mac's
  own on a Retina display.

The side lights sit on the right edge under POWER, as on the device: top to
bottom battery4, battery3, battery2, and the bottom light, red (battery0) or
green (battery1). They follow plastron's gauge: all four for a full battery
on the cable, a filling bar while charging, a red flash every five seconds
under 10% on the battery. The backlight dims the screen a little (enough to
see a level change, not so much that a low setting hides the picture) and
blacks it out when ZyperOS turns the panel off. Rumble shakes the device.

## Driving it from a shell

`simulator/simctl` talks to the open window over its control socket
(`~/Library/Caches/zyperos-qemu/NAME/app.sock`), one command and one JSON
answer, for checking ZyperOS and the window itself with no clicks:

```sh
simulator/simctl state                        # power, battery, cable, lights, backlight...
simulator/simctl tap start                    # 80 ms; tap a+right 500 for a chord
simulator/simctl press menu; simulator/simctl release menu
simulator/simctl battery 8; simulator/simctl cable 0
simulator/simctl wait-frames 10               # until ten more frames came
simulator/simctl fps                          # frames the window got in the last second
simulator/simctl latency down                 # a tap, and ms to the first frame it changed
simulator/simctl latency a 80 223 40 34       # ...in a part of the screen (a game's)
simulator/simctl keys 1                       # every button's key on it, as Option
simulator/simctl screenshot shot.png          # the VM's screen
simulator/simctl window-shot device.png       # the whole device, drawn
simulator/simctl power | shutdown | quit
```

`-n NAME` picks another window's VM. With no window open, `tools/qemu-run.sh
sim -n NAME battery 8` and the rest reach the same parts over SSH.

## How it works

The window is a SwiftUI app (`simulator/`, a Swift package: `SimCore` for the
parts with no window, `ZyperSimulator` for the app). It runs
`tools/qemu-run.sh start -n NAME --sound` and connects to two of the VM's
sockets:

- the screen, every frame the VM draws, from QEMU's D-Bus display
  (`-display dbus,p2p=yes`, which `tools/qemu-run.sh` starts headless VMs
  with), through memory shared with QEMU (SimCore's `DBusDisplayClient`).
  The window hands QEMU one end of a socket pair over `qmp.sock` (QMP's
  `getfd` and `add_client`), logs in on it as D-Bus's EXTERNAL, and gives
  `Console_0` a second pair's end as its display listener, one that says it
  takes `org.qemu.Display1.Listener.Unix.Map`. QEMU then sends `ScanoutMap`
  whenever the guest flips to another buffer, with the file descriptor of
  the memory it keeps that buffer in, which the window maps, and `UpdateMap`
  whenever the guest draws into it: a rectangle and no pixels. QEMU on a Mac
  has no ivshmem device (it needs Linux's eventfd), so this is the shared
  memory it offers. A QEMU that cannot share sends the pixels over the
  socket (`Scanout`, `Update`), which the window takes too.
- `vnc.sock`, QEMU's VNC server, where the VM has no D-Bus display (an older
  QEMU, `--window`) or it fails (the status line says why), or with
  `ZYPEROS_SIM_SCREEN=vnc`: raw frames (SimCore's `RFBClient`). QEMU's VNC
  server looks for changes on a timer of its own (30 ms) and backs off when
  one look sends nothing.

What the two give the window (2026-11-19, one VM, the same card, the Mac
busy with other builds; `simctl fps` and `simctl latency`, a press timed to
the first frame it changed as it reached the window, before the window
draws it):

| | VNC | D-Bus, shared memory |
|---|---|---|
| NES game (`ZyperOS Test.nes`) | 15 frames a second, a press shown in 11-45 ms | 60 frames a second, 4-11 ms |
| The home, a key held | 32 frames a second | 75 frames a second |
| The home, a press | 16-29 ms | 5-12 ms |
| MENU's menu, a key held | 31 frames a second | 75 frames a second |
| MENU's menu, a press | 4-36 ms | 9-12 ms |

The window takes about a tenth of one of the Mac's cores for 60 frames a
second. `ZYPEROS_SIM_DEBUG=1` prints each step of the D-Bus way in, and
each of QEMU's calls. `state` says which way the frames come
(`screen_path`: `shared memory`, `D-Bus` or `VNC`).
- `sim.sock`, QEMU's end of a virtio-serial port, whose other end is
  plastron's px2sim in the VM: lines both ways. The window sends `press a`,
  `release a`, `battery 15`, `cable 0`, `headphones 1`; px2sim sends `state
  leds=... backlight=... bl_power=... battery=... cable=... headphones=...`
  on every change, and `rumble STRONG WEAK MS` when the motor runs.

The device is drawn from the device files (`src/device/<id>_model.c`), the
same data desk mode draws from: `make handhelds` writes them into
`simulator/Sources/SimCore/Handhelds.swift`, and the window draws whatever
handheld it is given. The Pixel 2's are the measurements of the website's
3D model (`tools/build-pixel2-model.py`, the dimensioned drawing's
millimetres).

## Another handheld

`--device ID` (or `$ZYPEROS_SIM_DEVICE`) draws another handheld round the
screen, any of `Handhelds.swift`'s (`pixel2`, `brick`):

```sh
"simulator/build/ZyperOS Simulator.app/Contents/MacOS/ZyperSimulator" --device brick
```

Its keys, their keyboard keys, the panel's lights and its keyboard list
follow the handheld: the Brick has F1 and F2 (`[` and `]`, `brightdown` and
`brightup` over the socket) and no side lights; a button it has not is
refused (`simctl press` says so). [Building](building.md), "A handheld drawn
on the computer", says how a new handheld gets there: its model file, then
`make handhelds`.

The VM is still the Pixel 2's: plastron's `qemu_defconfig` builds the Pixel
2's system, and px2sim simulates the Pixel 2's parts. So today `--device
brick` is the Brick's drawing round a Pixel 2's screen, for seeing the
drawing; its picture is scaled to the Brick's glass. A VM for another
handheld is plastron's: a QEMU image of that handheld's system, and px2sim
taught its buttons and parts. Nothing in the window changes for it.

`swift test` in `simulator/` checks the line protocol and the control
commands.
