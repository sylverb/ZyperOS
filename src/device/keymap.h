/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_KEYMAP_H
#define ZYPEROS_KEYMAP_H

/* A computer's keyboard as a handheld's buttons: ONE map for desk mode (the
 * host build, src/device/desk.c), the Simulator (simulator/, App.swift's
 * KeyMap) and QEMU's own window (plastron's px2sim), by the keys' places
 * rather than their letters, so a hand learns it once and an AZERTY keyboard
 * has the same cluster under the same fingers. The emulators' usual one:
 * X and Z are A and B, S and A are X and Y, the shoulders along Q W E R.
 * tools/keymap-check.py holds this table and the Simulator's to the same
 * keys; docs/building.md and docs/simulator.md print it.
 *
 * Brightness has keys of its own here, [ and ], where a device has keys for
 * it (the Brick's F1 and F2); on the Pixel 2 it is MENU and the volume. */

#include <SDL.h>

#include "../inbutton.h"

static inline in_button keymap_button(SDL_Scancode sc)
{
	switch (sc) {
	case SDL_SCANCODE_LEFT:         return IN_LEFT;
	case SDL_SCANCODE_RIGHT:        return IN_RIGHT;
	case SDL_SCANCODE_UP:           return IN_UP;
	case SDL_SCANCODE_DOWN:         return IN_DOWN;
	case SDL_SCANCODE_X:            return IN_ACCEPT;
	case SDL_SCANCODE_Z:            return IN_BACK;
	case SDL_SCANCODE_S:            return IN_X;
	case SDL_SCANCODE_A:            return IN_Y;
	case SDL_SCANCODE_Q:            return IN_L1;
	case SDL_SCANCODE_W:            return IN_R1;
	case SDL_SCANCODE_E:            return IN_L2;
	case SDL_SCANCODE_R:            return IN_R2;
	case SDL_SCANCODE_RETURN:
	case SDL_SCANCODE_KP_ENTER:     return IN_START;
	case SDL_SCANCODE_RSHIFT:
	case SDL_SCANCODE_BACKSPACE:    return IN_SELECT;
	case SDL_SCANCODE_ESCAPE:
	case SDL_SCANCODE_M:            return IN_MENU;
	case SDL_SCANCODE_EQUALS:       return IN_VOLUP;
	case SDL_SCANCODE_MINUS:        return IN_VOLDN;
	case SDL_SCANCODE_LEFTBRACKET:  return IN_BRIGHTDN;
	case SDL_SCANCODE_RIGHTBRACKET: return IN_BRIGHTUP;
	case SDL_SCANCODE_P:            return IN_POWER;
	default:                        return IN_NONE;
	}
}

/* Each button's key, as the buttons under the desk window print it */
static inline const char *keymap_hint(in_button b)
{
	switch (b) {
	case IN_ACCEPT:   return "X";
	case IN_BACK:     return "Z";
	case IN_X:        return "S";
	case IN_Y:        return "A";
	case IN_L1:       return "Q";
	case IN_R1:       return "W";
	case IN_L2:       return "E";
	case IN_R2:       return "R";
	case IN_START:    return "Return";
	case IN_SELECT:   return "R-Shift";
	case IN_MENU:     return "Esc";
	case IN_VOLUP:    return "=";
	case IN_VOLDN:    return "-";
	case IN_BRIGHTDN: return "[";
	case IN_BRIGHTUP: return "]";
	case IN_POWER:    return "P";
	default:          return "";
	}
}

#endif
