/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_CARDART_H
#define ZYPEROS_CARDART_H

/* The card of a game with no art (DESIGN.md 11.1), in the part of it that is
 * plain C: what its ground is made of, and where its name breaks.
 *
 * The card is a box of its own, the same in both themes as a box is: a
 * gradient in the system's color, the texture of the system's era, a shape
 * and a second color picked by a hash of the name, and the system's own art
 * faint in a corner; or, once the game has been played, a frame of that play
 * (src/playframe.h), blurred and dimmed. Across it, the name: one treatment
 * on every card, so it is found the same way on every shelf - the title face,
 * hard left, its first line at the same height, at one size, near-white on a
 * dark band. The era is in the ground, never in the letters.
 *
 * No SDL here, so tools/cardart-check.c holds the rules without a window:
 * which texture each system gets, that a name always gets the same ground,
 * that a grey system still gets a color, and how a name breaks. */

#include <stdbool.h>
#include <stdint.h>

/* The texture of a system's era, by its tag (config/systems.cfg) */
typedef enum {
	CA_SHARDS,   /* polygons: PlayStation, N64, Saturn, Dreamcast, PSP... and the rest */
	CA_DITHER,   /* 8-bit: NES, Master System, PC Engine, Atari, PICO-8... */
	CA_SCAN,     /* 16-bit and arcade, on a CRT: Genesis, SNES, Neo Geo, arcade, PC */
	CA_LCD       /* handhelds: Game Boy, Game Gear, Lynx, WonderSwan, NDS... */
} ca_texture;

ca_texture cardart_texture(const char *tag);

/* A name's hash: FNV-1a, never 0. The same name, the same card, always. */
uint32_t cardart_hash(const char *name);

/* What a card's ground is, from the system's color and the name's hash */
typedef struct {
	unsigned c0, c1;          /* the gradient's ends, 0xRRGGBB: c0 where it starts */
	float    angle;           /* its direction, radians from straight down */
	float    glow_x, glow_y;  /* a soft light, in card fractions */
	int      shape;           /* 0 a band, 1 a disc, 2 two stripes */
	float    shape_x, shape_y, shape_r, shape_angle;
	unsigned shape_rgb;
	int      art_corner;      /* the system art: 0 bottom right, 1 bottom left, 2 top right */
	uint32_t seed;            /* the shards' layout */
} ca_ground;

/* `sys_rgb` CFG_NO_COLOR is not handled here: the caller gives the color it
 * draws the system's marks in. A grey (saturation under 15%) has no hue to
 * turn, so the hash mixes one of four in. */
void cardart_ground(unsigned sys_rgb, uint32_t hash, ca_ground *g);

/* Where the name sits: the card's own places (DESIGN.md 11.1), the one
 * source for the card (src/ui.c) and the Grid's cell (src/gamegrid.c), which
 * scales them to its own width and height */
#define CA_NAME_X   44   /* hard left, in a 512-wide card's pixels */
#define CA_NAME_TOP 38   /* the first line's capitals, in % of the card's height */
#define CA_CARD_W   512  /* the width CA_NAME_X and CA_SIZE are in */

/* ---- the name ----
 *
 * At CA_SIZE (in a 512-wide card's pixels) on every card, up to CA_LINES
 * lines; what follows " - " under it at 6/10 of that size in up to
 * CA_SUB_LINES. Smaller (CA_SIZES' next) only when one word is wider than the
 * box or the name would take more lines than that. Lines balanced (the
 * narrowest box that keeps their number), and never a word of three
 * characters or fewer alone on the last line ("2", "II", "64"): a narrower box
 * takes a word down to it at the same size, or where none can (a narrow Grid
 * cell), the next size. */
#define CA_SIZE 60
#define CA_SIZES 3
extern const int CA_SIZE_STEPS[CA_SIZES];   /* 60, 50, 42 */
#define CA_LINES 4
#define CA_SUB_LINES 2
#define CA_LINE 160

/* The width of `s` at `size` in pixels, in the face the card is set in */
typedef int (*ca_width_fn)(void *ctx, int size, const char *s);

typedef struct {
	int  size, sub_size;
	int  n, sub_n;
	char line[CA_LINES][CA_LINE];
	char sub[CA_SUB_LINES][CA_LINE];
} ca_name;

/* Breaks `name` for a box `box_w` wide, in at most `max_lines` lines (up to
 * CA_LINES; the card's is CA_LINES, a Grid cell's what its height holds) and
 * CA_SUB_LINES for the rest. False only for an empty name. At the smallest
 * size, a name that still does not fit keeps its first lines, the last one
 * ending in "..." (U+2026). */
bool cardart_name(const char *name, int box_w, int max_lines, ca_width_fn width, void *ctx,
                  ca_name *out);

#endif
