/* What the cover row does when it stops on a game's card with no art
 * (src/focusfx.h), without a screen:
 *
 * - nothing while the row moves;
 * - the sheen once across, on the turn curve over DS_MS_SHEEN, from the
 *   moment the row stops, then gone for as long as the focus stays: never
 *   a loop (a still shelf draws nothing, DESIGN.md 8);
 * - the sharp card over the blurred one, over DS_MS_SHARPEN from when it is
 *   in hand, then held; only on a played game's card;
 * - every one of them stopped at once by a press: the focus moving, or the
 *   row; the next card starts afresh;
 * - the redraw clock told when the last of them ends, and nothing asked of
 *   it once they have;
 * - and no sheen, no sharp card, on a card that is not a generated one.
 *
 * Links src/focusfx.c and NOT SDL.
 */
#include "../src/focusfx.h"
#include "../src/design.h"

#include <math.h>
#include <stdio.h>

static int fails;

static void ck(int cond, const char *what)
{
	if (!cond) { printf("  FAIL %s\n", what); fails++; }
}

int main(void)
{
	focusfx f;
	focusfx_out o;
	bool landed;

	printf("focusfx:\n");
	focusfx_reset(&f);

	/* the row moving onto it: nothing */
	landed = focusfx_step(&f, 3, 7, true, true, false, false, 1000, &o);
	ck(!landed && o.sheen < 0 && o.sharp == 0 && !o.moving && !o.until, "nothing while the row moves");

	/* it stops: the sheen starts, the sharp card is asked for */
	landed = focusfx_step(&f, 3, 7, true, true, true, false, 1100, &o);
	ck(landed, "the row stopping is the moment to ask for the sharp card");
	ck(o.sheen == 0 && o.moving && o.until == 1100 + DS_MS_SHEEN, "the sheen starts where the row stops");
	ck(o.sharp == 0, "no sharp card before it is in hand");
	landed = focusfx_step(&f, 3, 7, true, true, true, false, 1100 + DS_MS_SHEEN / 2, &o);
	ck(!landed, "asked once, not every frame");
	ck(fabsf(o.sheen - ds_turn(.5f)) < .001f, "half way at half the time, on the turn curve");

	/* the sharp card lands 100 ms in: over DS_MS_SHARPEN from there */
	focusfx_step(&f, 3, 7, true, true, true, true, 1200, &o);
	ck(o.sharp == 0 && o.until == 1100 + DS_MS_SHEEN, "the sharp card starts from nothing where it lands");
	focusfx_step(&f, 3, 7, true, true, true, true, 1200 + DS_MS_SHARPEN / 2, &o);
	ck(fabsf(o.sharp - .5f) < .01f, "half sharp half way");
	focusfx_step(&f, 3, 7, true, true, true, true, 1200 + DS_MS_SHARPEN, &o);
	ck(o.sharp == 1, "and sharp at its end");

	/* the sheen ends, once, and stays ended */
	focusfx_step(&f, 3, 7, true, true, true, true, 1100 + DS_MS_SHEEN, &o);
	ck(o.sheen < 0 && o.sharp == 1 && !o.moving && !o.until, "the sheen gone, the sharp card held, nothing to draw");
	focusfx_step(&f, 3, 7, true, true, true, true, 1100 + 10 * DS_MS_SHEEN, &o);
	ck(o.sheen < 0 && !o.moving, "and no second sheen while the focus stays: never a loop");

	/* a press mid-sheen: the next card, at once */
	focusfx_step(&f, 3, 8, true, true, false, false, 9000, &o);
	ck(o.sheen < 0 && o.sharp == 0 && !o.moving, "a press stops it all at once");
	focusfx_step(&f, 3, 8, true, true, true, false, 9050, &o);
	focusfx_step(&f, 3, 8, true, true, false, false, 9100, &o);
	ck(o.sheen < 0 && !o.moving, "the row moving again mid-sheen stops it");
	landed = focusfx_step(&f, 3, 8, true, true, true, false, 9300, &o);
	ck(landed && o.sheen == 0, "and it starts afresh where the row stops again");

	/* never played: a sheen, no sharp card */
	focusfx_reset(&f);
	focusfx_step(&f, 1, 1, true, false, true, true, 100, &o);
	focusfx_step(&f, 1, 1, true, false, true, true, 100 + DS_MS_SHARPEN * 2, &o);
	ck(o.sharp == 0 && o.sheen > 0, "a game never played: the sheen, and no sharp card");

	/* not a generated card (box art, a folder): nothing at all */
	focusfx_reset(&f);
	focusfx_step(&f, 1, 2, false, true, true, true, 100, &o);
	focusfx_step(&f, 1, 2, false, true, true, true, 200, &o);
	ck(o.sheen < 0 && o.sharp == 0 && !o.moving, "a cover with art: nothing");

	/* no focus */
	focusfx_reset(&f);
	ck(!focusfx_step(&f, -1, -1, false, false, true, false, 100, &o) && o.sheen < 0, "no focus: nothing");

	printf(fails ? "focusfx: %d FAILED\n" : "focusfx: ok\n", fails);
	return fails ? 1 : 0;
}
