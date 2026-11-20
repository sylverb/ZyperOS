/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_PKHOT_H
#define ZYPEROS_PKHOT_H

/* What portkeys (tools/portkeys.c) makes of SELECT and a button over your
 * PICO-8: the player's hotkeys (src/hotkeys.h, DESIGN.md 11.61), judged by
 * Diatom's own rule (diatom/src/chord.h) on the joypad's events, so a chord
 * that is a hotkey over a fake08 game is one over your PICO-8 and one that
 * is not is not: SELECT then the button, or the button a moment before
 * SELECT; SELECT then any other button, the d-pad too, is the game's.
 *
 * And what the cart is to see of the pad, as Diatom's core sees it: a chord
 * hidden, SELECT alone given back (chord.h's hide and give, pkhot_out),
 * which portkeys forwards to the pad PICO-8 reads while it holds the joypad
 * (tools/pkvpad.h). Only the acts portkeys can do are live (PKHOT_LIVE): the keys of Quick
 * Save, Quick Load, Faster and Slower are PICO-8's under SELECT, as a core's
 * that keeps no state are in Diatom.
 *
 * Pure: no device and no clock of its own. The events come in the order
 * the kernel queued them, one device, so no ordering across devices is
 * needed (tools/pkkeys.h has that for MENU and the volume keys). The codes
 * are the kernel's, written out, as pkkeys.h's are, so the check builds on
 * a computer. */

#include <stdbool.h>
#include <stdint.h>

#include "chord.h"

/* <linux/input.h>'s numbers for the Pixel 2 joypad's buttons */
#define PKHOT_BTN_SOUTH  0x130       /* A */
#define PKHOT_BTN_EAST   0x131       /* B */
#define PKHOT_BTN_NORTH  0x133       /* X */
#define PKHOT_BTN_WEST   0x134       /* Y */
#define PKHOT_BTN_TL     0x136
#define PKHOT_BTN_TR     0x137
#define PKHOT_BTN_TL2    0x138
#define PKHOT_BTN_TR2    0x139
#define PKHOT_BTN_SELECT 0x13a
#define PKHOT_BTN_START  0x13b
#define PKHOT_BTN_UP     0x220       /* BTN_DPAD_UP, then down, left, right */

/* The acts portkeys does over your PICO-8, 1u << chord_act */
#define PKHOT_LIVE ((1u << CHORD_SHOT) | (1u << CHORD_SHADER) | (1u << CHORD_DISPLAY) | \
                    (1u << CHORD_RESET) | (1u << CHORD_QUIT))

typedef struct {
	chord    c;
	uint32_t held;               /* the pad now, DIATOM_BIT(DIATOM_BTN_*) */
	uint32_t live;               /* the keys of PKHOT_LIVE's acts */
	bool     on;                 /* a table was given */
	uint32_t out;                /* what the game is to see now (pkhot_out) */
	uint64_t turn_ms;            /* when chord.h last took a turn */
} pkhot;

/* SELECT's rule takes its turns as the events come, and between them while
 * it waits to see whose a SELECT is, or gives one back (chord.h's CS_WAIT,
 * owed): every PKHOT_TICK_MS, where Diatom has a frame. chord.h gives a
 * SELECT let go at least two turns: 50 ms, more than a frame of a 30 fps
 * cart, which PICO-8 reads its pad once a frame for. */
#define PKHOT_TICK_MS 25

/* No hotkeys: every key the game's */
void pkhot_init(pkhot *h);

/* The table in force, in Diatom's RUN form (src/hotkeys.h's hk_run: "shot:y",
 * "save:r1,load:l1,faster:r2,slower:l2", "none"). False, and no hotkeys, for
 * one chord.h will not read. The pad's state is kept. */
bool pkhot_set(pkhot *h, const char *run);

/* The button a joypad code is, DIATOM_BIT(...), 0 for none of them */
uint32_t pkhot_bit(uint16_t code);

/* One event of the joypad, at `ms` (its own time, on the clock the devices
 * were set to): the acts it made, 1u << chord_act, only PKHOT_LIVE's. The
 * kernel's autorepeat (value 2) is no event. */
unsigned pkhot_key(pkhot *h, uint16_t code, int32_t value, uint64_t ms);

/* What the game is to see of the pad now, as Diatom's core sees it under
 * chord.h's rule: every button as it is, but SELECT while it waits (given
 * back as it is let go, or held on past CHORD_HOLD_MS), and SELECT and a
 * hotkey's button while they are a chord, until each is let go; MENU never,
 * which is ZyperOS's. With no live hotkeys, the pad as it is but MENU.
 * portkeys forwards this to the pad PICO-8 reads (tools/pkvpad.h), so a
 * chord never reaches the cart, as over a Diatom game. */
uint32_t pkhot_out(const pkhot *h);

/* A turn between events when one is due (pkhot_wait 0): the acts it made,
 * none in practice, and pkhot_out moved on (a SELECT given back, or held
 * long enough to be the game's) */
unsigned pkhot_tick(pkhot *h, uint64_t ms);

/* How long until pkhot_tick has a turn to take: -1 for none (nothing
 * waits), else the ms, 0 for now */
int pkhot_wait(const pkhot *h, uint64_t ms);

/* What is down on the joypad when its events cannot be trusted (after a
 * SYN_DROPPED, or the device gone, `held` 0): taken as it is, with no act,
 * and SELECT's rule starts again, so a release that was lost is never half
 * of a chord. */
void pkhot_resync(pkhot *h, uint32_t held);

#endif
