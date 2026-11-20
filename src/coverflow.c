/* SPDX-License-Identifier: GPL-3.0-only */
#include "artbadge_draw.h"
#include "clamp.h"
#include "coverflow.h"
#include "design.h"

#include <math.h>
#include <string.h>

/* End reflections one grid step above the count line. */
#define FLOOR_UNDER_COUNT ((690.0f - 8.0f) / 768.0f)

const cf_layout CF_LAYOUT_GAMES = {
	.size = 0.60f, .aspect = 0.72f, .step = 0.74f, .side_scale = 0.62f,
	.center_y = 0.47f, .tilt = 0.82f, .reflect = 1.52f,
	.side_alpha = 150, .strips = 16,
	.equal_area = true, .floor = FLOOR_UNDER_COUNT,
};

void cf_focus_rect(const cf_layout *lay, int screen_w, int screen_h, SDL_Rect *out)
{
	float ch = screen_h * lay->size;
	float cw = ch * lay->aspect;
	out->w = (int)cw;
	out->h = (int)ch;
	out->x = (int)(screen_w * 0.5f - cw * 0.5f);
	out->y = (int)(screen_h * lay->center_y - ch * 0.5f);
}

/* Every move takes the same time, whatever its distance: DESIGN.md 8.2's
 * **move**. Crossing the shelf is not a longer journey than stepping one card,
 * it is a faster one.
 *
 * This replaced an exponential ease toward the target with a speed cap over
 * it, and a teleport past six cards. The ease alone converged in time
 * proportional to log(distance), which was nearly constant already; the cap
 * made anything past a card and a half take time proportional to DISTANCE, so
 * a letter jump across a big shelf became a wait, and the teleport existed to
 * hide the worst of it. One duration removes the need for both. */
#define MOVE_MS ((float)DS_MS_MOVE)

/* The row glides only as far as the eye can follow in a move's time: at most
 * one card a frame at 60fps, on average. See CF_WARM_CARDS. */
_Static_assert(CF_WARM_CARDS * 1000 <= DS_MS_MOVE * 60,
               "a glide of CF_WARM_CARDS crosses more than a card a frame");

/* The clock every move is read from. See cf_clock. */
static Uint32 (*clock_fn)(void);

static Uint32 ticks(void) { return clock_fn ? clock_fn() : SDL_GetTicks(); }

void cf_clock(Uint32 (*now)(void)) { clock_fn = now; }

/* A row steps on **out** (ds_out), which spends most of its travel at once
 * and is why a single card reads as immediate though it takes as long as
 * forty.
 *
 * A cut's departure is the other, **in** (ds_in): it starts at rest and is at
 * its fastest when it ends. That is backwards for a step and exactly right for
 * a cut. The end of this move is where the discontinuity is, and a cut made
 * while the eye is tracking fast motion is close to invisible - it is the
 * ordinary cut on action - whereas one made at rest is the most visible cut
 * available, because the eye has settled and has nothing left to do but
 * notice it. */

static bool cf_loops(int count) { return count >= 2; }

void cf_reset(coverflow *cf, int cursor)
{
	memset(cf, 0, sizeof *cf);
	cf->pos = cf->from = cf->target = (float)cursor;
	cf->last_cursor = cursor;
	cf->primed = true;
}

void cf_set_cursor_dir(coverflow *cf, int cursor, int count, int dir)
{
	if (!cf->primed) { cf_reset(cf, cursor); return; }
	if (cursor == cf->last_cursor) return;

	bool loops = cf_loops(count);
	int raw = cursor - cf->last_cursor;
	if (loops) {
		/* take the shortest way around the ring so an end<->beginning move
		 * is a single smooth step, not a slide across the whole list */
		while (raw > count / 2) raw -= count;
		while (raw < -count / 2) raw += count;
		/* A ring of TWO has both neighbors one step away, so "shortest way
		 * around" has no answer and the rule above returns the literal
		 * difference every time: +1, -1, +1, -1. The row then rocks right and
		 * left rather than turning, however the cards are drawn - which is
		 * why this looked like a drawing bug and is not one. When the caller
		 * knows which way the press was, believe it over the arithmetic. */
		if (count == 2 && dir) raw = dir;
	}
	if (raw) cf->last_dir = raw > 0 ? 1 : -1;
	cf->last_cursor = cursor;

	/* A press arriving while a cut is still being drawn: the departure on
	 * screen belongs to a jump that is already over. Land it before taking
	 * the new one, or the shelf creeps while the cursor races - **in** is
	 * cubic, so at the 90ms key repeat a 160ms departure has covered under a
	 * fifth of its travel, and holding the button would leave the cards
	 * crawling under a title running through the list. */
	if (cf->cutting) {
		cf->pos = cf->target;
		cf->cutting = false;
	}
	cf->target += (float)raw;
	/* Start the tween from where the cards ARE, not from where the last one
	 * was headed. Holding a direction retargets every key repeat, and taking
	 * the live position each time is what makes that one continuous glide
	 * rather than a stutter back to the old start. */
	cf->from = cf->pos;
	/* A MOVE LONGER THAN THE WARM WINDOW IS NOT WORTH DRAWING.
	 *
	 * Not the same claim this made when it was written. Then it said such a
	 * move could not be drawn at all, because decoding sat on the render path
	 * and seven cold cards in a frame meant the frame never arrived - a letter
	 * jump showed the card it started near, then the card it landed on, and
	 * nothing between. texload.c fixed that, and the frames arrive now.
	 *
	 * What is left is two things the worker does not change. The cards out
	 * there have no texture yet, so drawing the move would fly past empty
	 * slots; and a fixed duration turns distance into speed, so past about
	 * nine cards it crosses more than one per frame and stops being motion at
	 * all. Both say the same thing: show the part that means something and
	 * cut the rest.
	 *
	 * The part that means something is the departure, because the warm cards
	 * are all behind the cursor - the destination is by definition the half
	 * not decoded yet. So the motion accelerates rather than settles and the
	 * cut lands at full speed, where the eye is least able to see it, while
	 * the caller warms the destination underneath; see cf_landing. */
	cf->cutting = false;
	{
		float span = cf->target - cf->from;
		float g = (float)CF_WARM_CARDS;
		if (span > (float)CF_WARM_CARDS || span < -(float)CF_WARM_CARDS) {
			cf->cutting = true;
			cf->cut_to = cf->from + (span > 0.0f ? g : -g);
		}
	}
	cf->t0 = ticks();
	cf->active = true;
}

void cf_stage(coverflow *cf, float from, float to, float u)
{
	float ms = MOVE_MS;

	if (u < 0.0f) u = 0.0f;
	if (u > 1.0f) u = 1.0f;
	cf->from = from;
	cf->target = to;
	cf->pos = from + (to - from) * ds_out(u);
	/* Backdated, so the move is already this far along by the clock the label
	 * and step_anim both measure against. */
	cf->t0 = ticks() - (Uint32)(u * ms);
	cf->active = true;
	cf->cutting = false;
	cf->primed = true;
	cf->last_cursor = (int)floorf(to + 0.5f);
}
