/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_PKKBD_H
#define ZYPEROS_PKKBD_H

/* A keyboard of portkeys's own, over your PICO-8, for the one key PICO-8
 * has that ZyperOS needs: CTRL-R, "Reload / Run / Restart cartridge" (its
 * manual), which Reset in the in-game menu and Reset Game's hotkey send.
 *
 * A virtual keyboard through the kernel's uinput (plastron's Pixel 2 kernel
 * has it built in, CONFIG_INPUT_UINPUT=y: PortMaster's gptokeyb makes its
 * keyboard the same way), with two keys, Left Ctrl and R. Both are among the
 * first 32 keys, which is what plastron's SDL takes for a keyboard without
 * udev (its patch 0003, evdev-scan-for-keyboards-without-udev), and SDL
 * watches /dev/input, so PICO-8 finds it whether it was made before PICO-8
 * started or after. Made as portkeys starts, kept until it ends.
 *
 * The keys are written as the kernel reads them from uinput: an input_event
 * each, then a report. CTRL goes down first and comes up last, held across
 * PICO-8's frames (PKKBD_HOLD_MS), so PICO-8 reads R with CTRL on whichever
 * way it asks. */

#include <stdbool.h>
#include <stdint.h>

/* <linux/input.h>'s numbers */
#define PKKBD_KEY_R        19
#define PKKBD_KEY_LEFTCTRL 29

/* How long the two keys are held: two of PICO-8's frames at 30 fps, and
 * more */
#define PKKBD_HOLD_MS 100

/* The keyboard, or -1 where the system has no uinput (and on a computer);
 * a line on stderr says why */
int pkkbd_open(const char *path);

/* CTRL-R on it: the presses, then after `hold` the let-goes. `nap` sleeps
 * `ms` (portkeys's own; a check's does nothing). False when a write failed. */
bool pkkbd_ctrl_r(int fd, void (*nap)(int ms));

void pkkbd_close(int fd);

#endif
