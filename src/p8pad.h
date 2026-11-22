/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_P8PAD_H
#define ZYPEROS_P8PAD_H

/* Your PICO-8's buttons, as the player remapped them (DESIGN.md 11.45 and
 * 11.55), pure: no SDL, no database.
 *
 * PICO-8 reads the pad through SDL's game controller mapping, which SDL
 * takes from SDL_GAMECONTROLLERCONFIG as it starts: O is SDL's "a", X its
 * "b", PICO-8's pause menu its "start". The PICO-8 shelf's buttons are kept
 * as every system's are (src/remapstore.h: map.PICO8, map.PICO8.<file>), in
 * the RetroPad's names fake08 reads them by: its B is O, its A is X
 * (src/controls.c). So the table a cart is sent (rms_sent) becomes one
 * mapping line for the Pixel 2's pad, each of the handheld's buttons on the
 * SDL button its target is: the RetroPad's bottom face (B) on SDL's bottom
 * one (a), its right (A) on SDL's right (b), and X, Y, the shoulders, START
 * and SELECT by their own names, as SDL's own mapping of this pad has them.
 * The table of nothing changed is the line sd/pixel2/PICO-8.sh had fixed
 * before (O on the B cap, X on the A cap: the device test, 2026-11-19), to
 * the byte.
 *
 * A button on None is on nothing; two buttons on one target are both on its
 * SDL button. What SDL's mapping cannot say is left plain: a turbo is the
 * button itself, and a layer (the N64's held+ pairs) is not sent. The d-pad
 * is never a button of the table and keeps its own four.
 *
 * The launcher writes the two lines, the joypad's and portkeys's pad's, to
 * P8_PAD_FILE as it hands over to PICO-8 (src/launch_screen.c) and again
 * when the in-game menu over it changed a button; PICO-8.sh exports them,
 * and the fixed ones when the file is not there or not these pads'. SDL
 * reads them once, at its start, so a change in the menu reaches PICO-8
 * when it starts again. */

#include <stdbool.h>
#include <stddef.h>

#include "remap.h"

/* The Pixel 2's pad as SDL names it (its GUID, its name) */
#define P8_PAD_GUID "190043d2010000000221000000010000"
#define P8_PAD_NAME "gkd_pixel2_joypad"
/* And the pad portkeys gives PICO-8 while it holds the joypad for the
 * hotkeys (tools/pkvpad.h): bus BUS_VIRTUAL, vendor "ZY", product 9, version
 * 1, the name's checksum left out (0000), which SDL matches whatever the
 * name's is. Its buttons are numbered as the joypad's, so the same body. */
#define P8_VPAD_GUID "06000000595a00000900000001000000"
#define P8_VPAD_NAME "ZyperOS pad for PICO-8"
/* Where the line goes, for PICO-8.sh (plat_flag on a computer) */
#define P8_PAD_FILE "/tmp/zyperos-pico8-pad"

/* The mapping line for `t`, SDL_GAMECONTROLLERCONFIG's form, into `out`:
 * the joypad's (p8_pad_line), or portkeys's pad's (p8_vpad_line). False when
 * it does not fit (`out` is then ""). */
bool p8_pad_line(const rm_table *t, char *out, size_t n);
bool p8_vpad_line(const rm_table *t, char *out, size_t n);

#endif
