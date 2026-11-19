/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_GAMEGRID_H
#define ZYPEROS_GAMEGRID_H

/* A shelf as a grid of covers (MENU > View > Grid, DESIGN.md 11.47): two
 * rows of cells of one shape, the focused cover ringed and lifted, its name
 * and its facts in a band under the grid, the keys on the shelf's foot. The
 * cover row's and the list's facts, handed in as theirs are.
 *
 * THE SHAPE COMES FROM THE ART. A shelf of 2:3 grids gets cells near 2:3,
 * five across; a shelf of square thumbnails four square cells; SNES boxes,
 * screenshots and 16:9 heroes three wide ones; tall PSP cases six narrow
 * ones; a shelf of every shape square cells (gg_shape). Art near the cell's
 * shape fills it, a sliver cut from its middle; art of another shape sits
 * whole in the cell over its own colors, blurred; a cut-out (a logo, a box
 * in 3D on nothing) whole on a plain card; a game with no art a card drawn
 * at the cell's shape (gg_draw).
 *
 * Left and Right step, coming round past the ends as the row does; Up and
 * Down a row, stopping at the top and the foot; L1 and R1 a screenful, round
 * the ends. src/shelf_keys.c.
 *
 * No I/O: the caller hands it its facts and its textures, as games_draw's
 * and gl_draw's are handed. */
#include "coverflow.h"

#include <SDL.h>
#include <stdbool.h>
#include <stdint.h>

#include "gridrule.h"

/* The grid on the canvas (`sw` x `sh`) for a shelf of `shape` */
gg_geom gg_geom_for(int sw, int sh, float shape);

/* ---- what is known of a cover beyond its texture ------------------------ */

/* Made when its picture arrives (gg_note), on the frame, from the picture
 * before it is freed: its colors blurred small, for art of another shape
 * than its cell's; whether it is a cut-out; whether it is the generated
 * card. Kept by the texture it was made for and a key of the game's (its
 * file's hash), so a slot that holds another picture since has none. */
typedef struct {
	SDL_Texture *of;
	unsigned     key;
	SDL_Texture *blur;
	bool         cutout, generated;
	/* A game's card as its cell shows it (the ground and the name over it),
	 * drawn once at the cell's size and copied after: what it was drawn
	 * for, its size, its name and color, the corners and the face, so a
	 * change of any draws it again (gamegrid.c's named_card) */
	SDL_Texture *card;
	int          card_w, card_h;
	unsigned     card_for;
	const void  *card_face;
} gg_art;

typedef struct {
	gg_art *a;
	int     n;
} gg_arts;

/* What cover `i` of a shelf of `count` is, from its picture `s` (NULL for
 * none at hand: what is known is that it is or is not the generated card) */
void gg_note(gg_arts *t, SDL_Renderer *r, int count, int i, SDL_Texture *of,
             unsigned key, SDL_Surface *s, bool generated);
/* Cover `i`'s, when it was made for `of` and `key`; NULL otherwise */
gg_art *gg_art_of(const gg_arts *t, int i, const SDL_Texture *of, unsigned key);
/* Cover `i`'s let go, with its textures (main.c's evict_far, evict_grid) */
void gg_drop(gg_arts *t, int i);
void gg_arts_free(gg_arts *t);
/* ---- the grid on a shelf ------------------------------------------------- */

typedef struct {
	bool     set;
	int      cursor, count, cols;
	float    top_from, top_to;     /* the first row in sight, in rows */
	float    fx_from, fy_from;     /* the ring's cell when its glide began */
	uint32_t moved, ring_moved;
	/* The shelf's shape: the one kept for it, or measured from its covers
	 * as they arrive, until it settles (GG_SETTLE_MS after it opened, or
	 * once every cover in sight is in); `measured` is what the covers in
	 * hand say, for the caller to keep for the shelf's next opening */
	float    shape, measured;
	uint32_t opened;
	bool     settled;
	/* What the last gg_draw drew: the first row in sight then, and how many
	 * cells in sight had no cover yet, for the caller's asks ahead and its
	 * eviction (gg_row_kept) */
	float    top_drawn;
	int      holes;
} gg_motion;

/* How long a shelf opened with no shape kept may take it from its covers */
#define GG_SETTLE_MS 1500

typedef struct {
	const char *heading;            /* the system, and the folder it is in */
	int count, cursor;
	unsigned accent;                /* the system's color */
	/* Row i's own system's color on a shelf of many systems' games; NULL
	 * for `accent` on every cover */
	unsigned (*accent_of)(void *ctx, int i);
	const char *(*title_of)(void *ctx, int i);
	int (*folder_of)(void *ctx, int i);
	bool (*fav_of)(void *ctx, int i);
	/* Cover i's texture if it is in hand, without asking for it, for the
	 * shape (cf_tex_fn's own asks) */
	cf_tex_fn peek;
	/* What is known of cover i (gg_note), or NULL; the grid keeps a game's
	 * card drawn at the cell's size in it */
	gg_art *(*art_of)(void *ctx, int i);
	/* The shape kept for this shelf, 0 for none */
	float kept;
	/* The focused game's video snap (src/snapshelf.h), as the cover row has
	 * it: its picture, its shape and the part of its texture the picture is
	 * in, how far it has opened (0 to 1), the wait before it, and whether
	 * the game has one, for the play mark */
	SDL_Texture *snap;
	int snap_w, snap_h;
	SDL_Rect snap_src;
	float snap_k, snap_wait;
	bool has_snap;
	/* The focused entry's facts, as the cover row has them */
	const char *played, *origin;
	bool has_achievements, resumable, install, inside;
	bool howto, opens;     /* Splore's entry (src/games.h) */
	int earned, total;
	/* START's word at the foot: what the shelf turns into next */
	const char *start_word;
} gg_info;

/* Reset to the cursor, with no motion and no shape: a shelf opened, a
 * folder changed, the View turned */
void gg_reset(gg_motion *m, int cursor, int count);
/* Whether it is moving at `now`, for when the focused entry's facts are
 * read */
bool gg_moving(const gg_motion *m, uint32_t now);
/* The columns of the shelf's grid now, for the keys */
int gg_cols(const gg_motion *m);

/* One frame. `texture` gives (and asks for) cover i's picture. Returns when
 * to draw again, as games_draw does. */
uint32_t gg_draw(gg_motion *m, SDL_Renderer *r, const gg_info *in,
                 cf_tex_fn texture, void *ctx, uint32_t now, uint32_t phase);

#endif
