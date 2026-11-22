/* SPDX-License-Identifier: GPL-3.0-only */
/* SELECT's hotkeys over your PICO-8: see pkhot.h. */
#include "pkhot.h"

#include <string.h>

void pkhot_init(pkhot *h)
{
	memset(h, 0, sizeof *h);
	chord_reset(&h->c);
}

bool pkhot_set(pkhot *h, const char *run)
{
	int key[CHORD_ACTS], i;

	h->on = false;
	h->live = 0;
	if (!run || !chord_keys_parse(run, key)) return false;
	chord_set_keys(&h->c, key);
	for (i = 0; i < CHORD_ACTS; i++)
		if (PKHOT_LIVE & (1u << i)) h->live |= chord_key_of(&h->c, (chord_act)i);
	h->on = true;
	return true;
}

uint32_t pkhot_bit(uint16_t code)
{
	switch (code) {
	case PKHOT_BTN_SOUTH:    return DIATOM_BIT(DIATOM_BTN_A);
	case PKHOT_BTN_EAST:     return DIATOM_BIT(DIATOM_BTN_B);
	case PKHOT_BTN_NORTH:    return DIATOM_BIT(DIATOM_BTN_X);
	case PKHOT_BTN_WEST:     return DIATOM_BIT(DIATOM_BTN_Y);
	case PKHOT_BTN_TL:       return DIATOM_BIT(DIATOM_BTN_L1);
	case PKHOT_BTN_TR:       return DIATOM_BIT(DIATOM_BTN_R1);
	case PKHOT_BTN_TL2:      return DIATOM_BIT(DIATOM_BTN_L2);
	case PKHOT_BTN_TR2:      return DIATOM_BIT(DIATOM_BTN_R2);
	case PKHOT_BTN_SELECT:   return DIATOM_BIT(DIATOM_BTN_SELECT);
	case PKHOT_BTN_START:    return DIATOM_BIT(DIATOM_BTN_START);
	case PKHOT_BTN_UP:       return DIATOM_BIT(DIATOM_BTN_UP);
	case PKHOT_BTN_UP + 1:   return DIATOM_BIT(DIATOM_BTN_DOWN);
	case PKHOT_BTN_UP + 2:   return DIATOM_BIT(DIATOM_BTN_LEFT);
	case PKHOT_BTN_UP + 3:   return DIATOM_BIT(DIATOM_BTN_RIGHT);
	/* MENU, BTN_TRIGGER_HAPPY1: Diatom's, and here portkeys's own */
	case 0x2c0:              return DIATOM_BIT(DIATOM_BTN_MENU);
	default:                 return 0;
	}
}

#define MENU_BIT DIATOM_BIT(DIATOM_BTN_MENU)
#define SEL_BIT  DIATOM_BIT(DIATOM_BTN_SELECT)

/* One of chord.h's turns, the pad having gone from `was` to h->held: what
 * the game sees from it on, and the acts. With no live hotkeys no rule at
 * all: the pad as it is. */
static unsigned turn(pkhot *h, uint32_t was, uint64_t ms)
{
	chord_out o;

	if (!h->on || !h->live) {
		h->out = h->held & ~MENU_BIT;
		return 0;
	}
	o = chord_turn(&h->c, h->held, was, ms, h->live);
	h->out = ((h->held & ~o.hide) | o.give) & ~MENU_BIT;
	h->turn_ms = ms;
	return o.acts & PKHOT_LIVE;
}

unsigned pkhot_key(pkhot *h, uint16_t code, int32_t value, uint64_t ms)
{
	const uint32_t bit = pkhot_bit(code), was = h->held;

	if (!bit || value == 2) return 0;
	if (value) h->held |= bit;
	else h->held &= ~bit;
	if (h->held == was) return 0;
	/* A turn for every change, as a frame of Diatom's would see it */
	return turn(h, was, ms);
}

uint32_t pkhot_out(const pkhot *h)
{
	return h->out;
}

int pkhot_wait(const pkhot *h, uint64_t ms)
{
	int64_t left;

	if (!h->on || !h->live) return -1;
	/* SELECT down and not the game's yet (waiting, or a chord's), or one
	 * still being given back, the turn that takes it back too (the game
	 * still has it, let go) */
	if (!(((h->held & SEL_BIT) && !(h->out & SEL_BIT)) || ((h->out & SEL_BIT) && !(h->held & SEL_BIT)) ||
	      h->c.owed > 0 || h->c.gap))
		return -1;
	left = (int64_t)(h->turn_ms + PKHOT_TICK_MS) - (int64_t)ms;
	return left < 0 ? 0 : (int)left;
}

unsigned pkhot_tick(pkhot *h, uint64_t ms)
{
	if (pkhot_wait(h, ms) != 0) return 0;
	return turn(h, h->held, ms);
}

void pkhot_resync(pkhot *h, uint32_t held)
{
	int key[CHORD_ACTS];

	memcpy(key, h->c.key, sizeof key);
	chord_reset(&h->c);
	chord_set_keys(&h->c, key);
	/* SELECT found down is no fresh press: it is the game's until it is let
	 * go, as SELECT pressed with another button is (chord.h), so no button
	 * under it is a hotkey until then. Told so as chord_turn hears it, a
	 * press with a button that is no hotkey's, the d-pad's Up, beside it */
	if (held & DIATOM_BIT(DIATOM_BTN_SELECT))
		chord_turn(&h->c, held | DIATOM_BIT(DIATOM_BTN_UP), 0, 0, 0);
	h->held = held;
	/* ...and the game sees the pad as it is, nothing hidden or owed */
	h->out = held & ~MENU_BIT;
}
