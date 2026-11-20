/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_PKVPAD_H
#define ZYPEROS_PKVPAD_H

/* The pad your PICO-8 reads while the player has hotkeys over it: a pad of
 * portkeys's own through the kernel's uinput, which portkeys feeds with the
 * joypad's buttons as chord.h lets the game see them (tools/pkhot.h's
 * pkhot_out), while it holds the joypad itself (EVIOCGRAB). So a hotkey's
 * chord, SELECT and its button, never reaches the cart, as over a Diatom
 * game, and every other press does, at once, in its order.
 *
 * Its buttons are the joypad's but MENU, so SDL numbers them as it numbers
 * the joypad's (b0 the A cap, b8 SELECT, b9 START, b10 to b13 the d-pad) and
 * one mapping body fits both (src/p8pad.h, P8_VPAD_GUID). Buttons only, as
 * the joypad: plastron's SDL takes that for a joystick (its patch 0001).
 * PICO-8 merges its first two controllers into its first player
 * (merge_joysticks 2, which PICO-8.sh sets), so the joypad, silent while it
 * is held, and this pad are one player 1.
 *
 * The writes are input_events as uinput reads them, one per button that
 * changed and a report after them. Pure but for the device: a check writes
 * into a pipe. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* What the pad says it is, as SDL reads it into its GUID (bus BUS_VIRTUAL,
 * vendor "ZY", product, version): src/p8pad.h's P8_VPAD_GUID */
#define PKVPAD_NAME    "ZyperOS pad for PICO-8"
#define PKVPAD_VENDOR  0x5a59
#define PKVPAD_PRODUCT 0x0009
#define PKVPAD_VERSION 1

/* The pad, or -1 where the system has no uinput (and on a computer); a line
 * on stderr says why. Its node (/dev/input/eventN) into `node`, "" when it
 * cannot be found: what portkeys looks for among PICO-8's open files before
 * it holds the joypad. */
int pkvpad_open(const char *path, char *node, size_t n);

/* The buttons that differ between `was` and `now` (DIATOM_BIT masks, as
 * tools/pkhot.h counts them), each pressed or let go, then a report; nothing
 * when none differ. False when a write failed. */
bool pkvpad_put(int fd, uint32_t was, uint32_t now);

void pkvpad_close(int fd);

#endif
